// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/companion/text_finder/text_highlighter_manager.h"

#include "chrome/browser/companion/text_finder/text_finder_manager_base_test.h"
#include "chrome/browser/companion/text_finder/text_highlighter.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/page.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/annotation/annotation.mojom.h"
#include "ui/gfx/geometry/rect.h"

namespace companion {

namespace {

constexpr char kBaseUrl[] = "https://www.example.com/";

}  // namespace

class TextHighlighterManagerTest : public content::RenderViewHostTestHarness {
 public:
  TextHighlighterManagerTest() = default;
  ~TextHighlighterManagerTest() override = default;

 protected:
  // Implements `content::RenderViewHostTestHarness`.
  void SetUp() override { content::RenderViewHostTestHarness::SetUp(); }

  void TearDown() override {
    // Owned web contentses must be destroyed before the test harness.
    web_contents_list_.clear();
    content::RenderViewHostTestHarness::TearDown();
  }

  std::unique_ptr<content::BrowserContext> CreateBrowserContext() override {
    return std::make_unique<TestingProfile>();
  }

  // Creates a new manager attached to mock pages.
  TextHighlighterManager* CreateManagerForTest(
      MockAnnotationAgentContainer* mock_container) {
    // Create a test frame and navigate it to a unique URL.
    std::unique_ptr<content::WebContents> wc = CreateTestWebContents();
    content::RenderFrameHostTester::For(wc->GetPrimaryMainFrame())
        ->InitializeRenderFrameIfNeeded();
    content::NavigationSimulator::NavigateAndCommitFromBrowser(
        wc.get(),
        GURL(kBaseUrl + base::NumberToString(web_contents_list_.size())));

    std::unique_ptr<service_manager::InterfaceProvider::TestApi> test_api;
    if (mock_container) {
      CHECK(!mock_container->is_bound());
      test_api = std::make_unique<service_manager::InterfaceProvider::TestApi>(
          wc->GetPrimaryMainFrame()->GetRemoteInterfaces());
      test_api->SetBinderForName(
          blink::mojom::AnnotationAgentContainer::Name_,
          base::BindRepeating(&MockAnnotationAgentContainer::Bind,
                              base::Unretained(mock_container)));
    }

    // Create and attach a `TextHighlighterManager` to the primary page.
    content::Page& page = wc->GetPrimaryPage();
    TextHighlighterManager::CreateForPage(page);
    TextHighlighterManager* manager = TextHighlighterManager::GetForPage(page);
    DCHECK(manager);
    web_contents_list_.emplace_back(std::move(wc));

    CHECK(!mock_container || mock_container->is_bound());

    return manager;
  }

  std::vector<std::unique_ptr<content::WebContents>> web_contents_list_;
};

TEST_F(TextHighlighterManagerTest, TextHighlighterTest) {
  // Set up a manager bound to the mock agent container.
  MockAnnotationAgentContainer mock_agent_container;
  TextHighlighterManager* manager = CreateManagerForTest(&mock_agent_container);

  // Create a new text highlighter.
  const std::string text_directive_1 = "ab,cd";
  manager->CreateTextHighlighterAndRemoveExistingInstance(text_directive_1);
  EXPECT_THAT(manager->text_highlighter_->GetTextDirective(), text_directive_1);

  // Create a second instance and automatically delete the existing one.
  const std::string text_directive_2 = "bcd";
  manager->CreateTextHighlighterAndRemoveExistingInstance(text_directive_2);
  EXPECT_THAT(manager->text_highlighter_->GetTextDirective(), text_directive_2);
}

}  // namespace companion
