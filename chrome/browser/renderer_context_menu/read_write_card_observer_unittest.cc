// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/read_write_card_observer.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/ui/chromeos/read_write_cards/read_write_card_controller.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/context_menu_params.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom-shared.h"
#include "ui/gfx/geometry/rect.h"

namespace {

// Returns a test context menu.
std::unique_ptr<TestRenderViewContextMenu> CreateContextMenu(
    content::WebContents* web_contents) {
  auto menu = std::make_unique<TestRenderViewContextMenu>(
      *web_contents->GetPrimaryMainFrame(), content::ContextMenuParams());
  return menu;
}

class TestReadWriteCardController : public chromeos::ReadWriteCardController {
 public:
  TestReadWriteCardController() = default;

  TestReadWriteCardController(const TestReadWriteCardController&) = delete;
  TestReadWriteCardController& operator=(const TestReadWriteCardController&) =
      delete;

  ~TestReadWriteCardController() override = default;

  // chromeos::ReadWriteCardController:
  void OnContextMenuShown(Profile* profile) override {}

  void OnTextAvailable(const gfx::Rect& anchor_bounds,
                       const std::string& selected_text,
                       const std::string& surrounding_text) override {}

  void OnAnchorBoundsChanged(const gfx::Rect& anchor_bounds) override {
    on_anchor_bounds_changed_called_ = true;
  }

  void OnDismiss(bool is_other_command_executed) override {
    on_dismiss_called_ = true;
  }

  base::WeakPtr<TestReadWriteCardController> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  bool on_anchor_bounds_changed_called() {
    return on_anchor_bounds_changed_called_;
  }
  bool on_dismiss_called() { return on_dismiss_called_; }

 private:
  bool on_anchor_bounds_changed_called_ = false;
  bool on_dismiss_called_ = false;

  base::WeakPtrFactory<TestReadWriteCardController> weak_factory_{this};
};

}  // namespace

class ReadWriteCardObserverTest : public ChromeRenderViewHostTestHarness {
 public:
  ReadWriteCardObserverTest() = default;

  ReadWriteCardObserverTest(const ReadWriteCardObserverTest&) = delete;
  ReadWriteCardObserverTest& operator=(const ReadWriteCardObserverTest&) =
      delete;

  ~ReadWriteCardObserverTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    context_menu_ = CreateContextMenu(web_contents());
    observer_ =
        std::make_unique<ReadWriteCardObserver>(context_menu_.get(), profile());

    auto controllers = InitControllers();

    observer_->OnFetchControllers(content::ContextMenuParams(),
                                  controllers);
  }

  void TearDown() override {
    observer_.reset();
    context_menu_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  std::vector<raw_ptr<chromeos::ReadWriteCardController>>
  GetReadWriteCardControllers() {
    return observer_->read_write_card_controllers_;
  }

 protected:
  std::vector<std::unique_ptr<TestReadWriteCardController>> controllers_;
  std::unique_ptr<ReadWriteCardObserver> observer_;

 private:
  std::vector<base::WeakPtr<chromeos::ReadWriteCardController>>
  InitControllers() {
    std::vector<base::WeakPtr<chromeos::ReadWriteCardController>> result;

    for (int i = 0; i < 2; ++i) {
      auto controller = std::make_unique<TestReadWriteCardController>();
      result.emplace_back(controller->GetWeakPtr());
      controllers_.emplace_back(std::move(controller));
    }

    return result;
  }

  std::unique_ptr<custom_handlers::ProtocolHandlerRegistry> registry_;
  std::unique_ptr<TestRenderViewContextMenu> context_menu_;
};

// Make sure that all controllers are fetched into the class after
// `OnFetchControllers`.
TEST_F(ReadWriteCardObserverTest, FetchController) {
  EXPECT_EQ(controllers_.size(), GetReadWriteCardControllers().size());
  EXPECT_FALSE(controllers_.empty());

  for (size_t i = 0; i < controllers_.size(); ++i) {
    EXPECT_EQ(controllers_[i].get(), GetReadWriteCardControllers()[i]);
  }
}

TEST_F(ReadWriteCardObserverTest, BoundsChanged) {
  observer_->OnContextMenuViewBoundsChanged(/*bounds_in_screen=*/gfx::Rect());
  for (auto& controller : controllers_) {
    EXPECT_TRUE(controller->on_anchor_bounds_changed_called());
  }
}

TEST_F(ReadWriteCardObserverTest, OnMenuClosed) {
  observer_->OnMenuClosed();
  for (auto& controller : controllers_) {
    EXPECT_TRUE(controller->on_dismiss_called());
  }
}
