// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tab_annotation_manager.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/annotation/annotation.mojom.h"
#include "ui/gfx/geometry/rect.h"

namespace actor {
namespace {

constexpr gfx::Rect kTestRect(10, 10, 100, 20);

class FakeAnnotationAgentContainer
    : public blink::mojom::AnnotationAgentContainer,
      public blink::mojom::AnnotationAgent {
 public:
  FakeAnnotationAgentContainer()
      : container_receiver_(this), agent_receiver_(this) {}
  ~FakeAnnotationAgentContainer() override = default;

  void Bind(mojo::ScopedMessagePipeHandle handle) {
    container_receiver_.reset();
    container_receiver_.Bind(
        mojo::PendingReceiver<blink::mojom::AnnotationAgentContainer>(
            std::move(handle)));
  }

  void NotifyAttachment(const gfx::Rect& rect,
                        blink::mojom::AttachmentResult result) {
    ASSERT_TRUE(host_remote_.is_bound());
    host_remote_->DidFinishAttachment(rect, result);
    host_remote_.FlushForTesting();
  }

  void DisconnectAgent() { agent_receiver_.reset(); }

  void SetOnAgentDisconnectedCallback(base::OnceClosure callback) {
    CHECK(agent_receiver_.is_bound());
    CHECK(!on_agent_disconnected_);
    on_agent_disconnected_ = std::move(callback);
    agent_receiver_.set_disconnect_handler(
        base::BindOnce(&FakeAnnotationAgentContainer::OnAgentDisconnected,
                       base::Unretained(this)));
  }

  void WaitForAgentDisconnected() {
    CHECK(agent_receiver_.is_bound());
    base::test::TestFuture<void> future;
    SetOnAgentDisconnectedCallback(future.GetCallback());
    EXPECT_TRUE(future.Wait());
  }

  void set_on_create_agent(base::OnceClosure callback) {
    on_create_agent_ = std::move(callback);
  }

  bool scroll_into_view_called() const { return scroll_into_view_called_; }
  bool last_applies_focus() const { return last_applies_focus_; }
  const blink::mojom::SelectorPtr& last_selector() const {
    return last_selector_;
  }
  std::optional<blink::mojom::AnnotationType> last_annotation_type() const {
    return last_annotation_type_;
  }

 private:
  // blink::mojom::AnnotationAgentContainer:
  void CreateAgent(
      mojo::PendingRemote<blink::mojom::AnnotationAgentHost> host_remote,
      mojo::PendingReceiver<blink::mojom::AnnotationAgent> agent_receiver,
      blink::mojom::AnnotationType type,
      blink::mojom::SelectorPtr selector,
      std::optional<int32_t> search_range_start_node_id) override {
    last_annotation_type_ = type;
    last_selector_ = std::move(selector);

    host_remote_.reset();
    host_remote_.Bind(std::move(host_remote));

    agent_receiver_.reset();
    agent_receiver_.Bind(std::move(agent_receiver));

    if (on_create_agent_) {
      std::move(on_create_agent_).Run();
    }
  }

  void CreateAgentFromSelection(
      blink::mojom::AnnotationType type,
      CreateAgentFromSelectionCallback callback) override {}
  void RemoveAgentsOfType(blink::mojom::AnnotationType type) override {}

  // blink::mojom::AnnotationAgent:
  void ScrollIntoView(bool applies_focus) override {
    scroll_into_view_called_ = true;
    last_applies_focus_ = applies_focus;
  }

  void OnAgentDisconnected() {
    if (on_agent_disconnected_) {
      std::move(on_agent_disconnected_).Run();
    }
  }

  mojo::Receiver<blink::mojom::AnnotationAgentContainer> container_receiver_;
  mojo::Receiver<blink::mojom::AnnotationAgent> agent_receiver_;
  mojo::Remote<blink::mojom::AnnotationAgentHost> host_remote_;

  base::OnceClosure on_create_agent_;
  base::OnceClosure on_agent_disconnected_;
  bool scroll_into_view_called_ = false;
  bool last_applies_focus_ = false;
  blink::mojom::SelectorPtr last_selector_;
  std::optional<blink::mojom::AnnotationType> last_annotation_type_;
};

class TabAnnotationManagerTest : public ChromeRenderViewHostTestHarness {
 public:
  TabAnnotationManagerTest() = default;
  ~TabAnnotationManagerTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    NavigateAndCommit(GURL("https://example.com"));

    TabAnnotationManager::CreateForWebContents(web_contents());
    manager_ = TabAnnotationManager::FromWebContents(web_contents());
    ASSERT_NE(manager_, nullptr);

    service_manager::InterfaceProvider::TestApi test_api(
        main_rfh()->GetRemoteInterfaces());
    test_api.SetBinderForName(
        blink::mojom::AnnotationAgentContainer::Name_,
        base::BindRepeating(&FakeAnnotationAgentContainer::Bind,
                            base::Unretained(&fake_container_)));
  }

  void TearDown() override {
    manager_ = nullptr;
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  raw_ptr<TabAnnotationManager> manager_ = nullptr;
  FakeAnnotationAgentContainer fake_container_;
};

TEST_F(TabAnnotationManagerTest, EmptyQueryFails) {
  base::test::TestFuture<bool> future;
  manager_->HighlightText("", future.GetCallback());
  EXPECT_FALSE(future.Get());
  EXPECT_FALSE(manager_->HasActiveHighlight());
}

TEST_F(TabAnnotationManagerTest, Highlight) {
  base::test::TestFuture<void> create_future;
  fake_container_.set_on_create_agent(create_future.GetCallback());

  base::test::TestFuture<bool> highlight_future;
  manager_->HighlightText("search query", highlight_future.GetCallback());

  EXPECT_TRUE(create_future.Wait());
  EXPECT_EQ(fake_container_.last_annotation_type(),
            blink::mojom::AnnotationType::kGlic);
  ASSERT_TRUE(fake_container_.last_selector());
  ASSERT_TRUE(fake_container_.last_selector()->is_serialized_selector());
  EXPECT_EQ(fake_container_.last_selector()->get_serialized_selector(),
            "search%20query");

  fake_container_.NotifyAttachment(kTestRect,
                                   blink::mojom::AttachmentResult::kSuccess);

  EXPECT_TRUE(highlight_future.Get());
  EXPECT_TRUE(fake_container_.scroll_into_view_called());
  EXPECT_TRUE(fake_container_.last_applies_focus());
  EXPECT_TRUE(manager_->HasActiveHighlight());
}

TEST_F(TabAnnotationManagerTest, HighlightFailsWhenNotFound) {
  base::test::TestFuture<void> create_future;
  fake_container_.set_on_create_agent(create_future.GetCallback());

  base::test::TestFuture<bool> highlight_future;
  manager_->HighlightText("missing query", highlight_future.GetCallback());

  EXPECT_TRUE(create_future.Wait());
  fake_container_.NotifyAttachment(
      gfx::Rect(), blink::mojom::AttachmentResult::kSelectorNotMatched);

  EXPECT_FALSE(highlight_future.Get());
  EXPECT_FALSE(fake_container_.scroll_into_view_called());
  EXPECT_FALSE(manager_->HasActiveHighlight());
}

TEST_F(TabAnnotationManagerTest, ClearHighlight) {
  base::test::TestFuture<void> create_future;
  fake_container_.set_on_create_agent(create_future.GetCallback());

  base::test::TestFuture<bool> highlight_future;
  manager_->HighlightText("search query", highlight_future.GetCallback());

  EXPECT_TRUE(create_future.Wait());
  fake_container_.NotifyAttachment(kTestRect,
                                   blink::mojom::AttachmentResult::kSuccess);
  EXPECT_TRUE(highlight_future.Get());
  EXPECT_TRUE(manager_->HasActiveHighlight());

  manager_->ClearHighlight();
  fake_container_.WaitForAgentDisconnected();
  EXPECT_FALSE(manager_->HasActiveHighlight());
}

TEST_F(TabAnnotationManagerTest, PrimaryPageChangedClearsHighlight) {
  base::test::TestFuture<void> create_future;
  fake_container_.set_on_create_agent(create_future.GetCallback());

  base::test::TestFuture<bool> highlight_future;
  manager_->HighlightText("search query", highlight_future.GetCallback());

  EXPECT_TRUE(create_future.Wait());
  fake_container_.NotifyAttachment(kTestRect,
                                   blink::mojom::AttachmentResult::kSuccess);
  EXPECT_TRUE(highlight_future.Get());
  EXPECT_TRUE(manager_->HasActiveHighlight());

  NavigateAndCommit(GURL("https://example.com/another_page"));
  fake_container_.WaitForAgentDisconnected();
  EXPECT_FALSE(manager_->HasActiveHighlight());
}

TEST_F(TabAnnotationManagerTest, AgentDisconnectedClearsHighlight) {
  base::test::TestFuture<void> create_future;
  fake_container_.set_on_create_agent(create_future.GetCallback());

  base::test::TestFuture<bool> highlight_future;
  manager_->HighlightText("search query", highlight_future.GetCallback());

  EXPECT_TRUE(create_future.Wait());
  fake_container_.NotifyAttachment(kTestRect,
                                   blink::mojom::AttachmentResult::kSuccess);
  EXPECT_TRUE(highlight_future.Get());
  EXPECT_TRUE(manager_->HasActiveHighlight());

  fake_container_.DisconnectAgent();
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return !manager_->HasActiveHighlight(); }));
}

TEST_F(TabAnnotationManagerTest, ReplacesExistingHighlight) {
  base::test::TestFuture<void> create_future1;
  fake_container_.set_on_create_agent(create_future1.GetCallback());

  base::test::TestFuture<bool> highlight_future1;
  manager_->HighlightText("first query", highlight_future1.GetCallback());

  EXPECT_TRUE(create_future1.Wait());
  fake_container_.NotifyAttachment(kTestRect,
                                   blink::mojom::AttachmentResult::kSuccess);
  EXPECT_TRUE(highlight_future1.Get());
  EXPECT_TRUE(manager_->HasActiveHighlight());

  base::test::TestFuture<void> agent_disconnect_future;
  fake_container_.SetOnAgentDisconnectedCallback(
      agent_disconnect_future.GetCallback());

  base::test::TestFuture<void> create_future2;
  fake_container_.set_on_create_agent(create_future2.GetCallback());

  base::test::TestFuture<bool> highlight_future2;
  manager_->HighlightText("second query", highlight_future2.GetCallback());

  EXPECT_TRUE(create_future2.Wait());
  EXPECT_TRUE(agent_disconnect_future.Wait());
  ASSERT_TRUE(fake_container_.last_selector());
  EXPECT_EQ(fake_container_.last_selector()->get_serialized_selector(),
            "second%20query");

  fake_container_.NotifyAttachment(kTestRect,
                                   blink::mojom::AttachmentResult::kSuccess);
  EXPECT_TRUE(highlight_future2.Get());
  EXPECT_TRUE(manager_->HasActiveHighlight());
}

TEST_F(TabAnnotationManagerTest, HighlightCancelledBySubsequentRequest) {
  base::test::TestFuture<void> create_future1;
  fake_container_.set_on_create_agent(create_future1.GetCallback());

  base::test::TestFuture<bool> highlight_future1;
  manager_->HighlightText("first query", highlight_future1.GetCallback());

  EXPECT_TRUE(create_future1.Wait());

  base::test::TestFuture<void> create_future2;
  fake_container_.set_on_create_agent(create_future2.GetCallback());

  base::test::TestFuture<bool> highlight_future2;
  // Second request before first attachment finishes cancels the first.
  manager_->HighlightText("second query", highlight_future2.GetCallback());

  EXPECT_FALSE(highlight_future1.Get());

  EXPECT_TRUE(create_future2.Wait());
  fake_container_.NotifyAttachment(kTestRect,
                                   blink::mojom::AttachmentResult::kSuccess);
  EXPECT_TRUE(highlight_future2.Get());
  EXPECT_TRUE(manager_->HasActiveHighlight());
}

}  // namespace
}  // namespace actor
