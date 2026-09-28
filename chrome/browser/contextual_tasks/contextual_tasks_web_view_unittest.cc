// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_web_view.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_cookie_synchronizer.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_eligibility_manager.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ghost_loader_view.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_ui_service_delegate.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/contextual_tasks/public/features.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"
#include "ui/shell_dialogs/fake_select_file_dialog.h"
#include "ui/shell_dialogs/select_file_dialog.h"

using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;

namespace contextual_tasks {
namespace {

class FakeContextualTasksUiService : public ContextualTasksUiService {
 public:
  explicit FakeContextualTasksUiService(Profile* profile)
      : ContextualTasksUiService(
            profile,
            std::make_unique<NiceMock<MockContextualTasksUiServiceDelegate>>(),
            /*contextual_tasks_service=*/nullptr,
            /*identity_manager=*/nullptr,
            /*aim_eligibility_service=*/nullptr,
            /*eligibility_manager=*/nullptr,
            /*cookie_synchronizer=*/nullptr) {}

  bool IsAiUrl(const GURL& url) override {
    return url.host() == "ai.google.com";
  }
};

class ContextualTasksWebViewTest : public testing::Test {
 public:
  void SetUp() override {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{kContextualTasks,
                              kContextualTasksSidePanelRearchitecture},
        /*disabled_features=*/{});

    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("testing_profile");
    browser_window_ = std::make_unique<NiceMock<MockBrowserWindowInterface>>();

    ON_CALL(*browser_window_, GetProfile()).WillByDefault(Return(profile_));
    ON_CALL(*browser_window_, GetFeatures())
        .WillByDefault(ReturnRef(browser_window_features_));
    ON_CALL(*browser_window_, GetUnownedUserDataHost())
        .WillByDefault(ReturnRef(unowned_user_data_host_));

    ContextualTasksUiServiceFactory::GetInstance()->SetTestingFactory(
        profile_, base::BindRepeating([](content::BrowserContext* context)
                                          -> std::unique_ptr<KeyedService> {
          return std::make_unique<FakeContextualTasksUiService>(
              Profile::FromBrowserContext(context));
        }));
  }

  void TearDown() override {
    web_view_.reset();
    browser_window_.reset();
    profile_ = nullptr;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  variations::test::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  base::test::ScopedFeatureList feature_list_;
  raw_ptr<TestingProfile> profile_ = nullptr;
  BrowserWindowFeatures browser_window_features_;
  ui::UnownedUserDataHost unowned_user_data_host_;
  std::unique_ptr<NiceMock<MockBrowserWindowInterface>> browser_window_;
  std::unique_ptr<ContextualTasksWebView> web_view_;
};

TEST_F(ContextualTasksWebViewTest, InitializationWithRearchitecture) {
  web_view_ = std::make_unique<ContextualTasksWebView>(browser_window_.get());

  EXPECT_NE(web_view_->toolbar_web_view(), nullptr);
  EXPECT_NE(web_view_->content_web_view(), nullptr);
  EXPECT_NE(web_view_->ghost_loader_view(), nullptr);
  EXPECT_FALSE(web_view_->IsGhostLoaderVisible());
}

TEST_F(ContextualTasksWebViewTest, SetGhostLoaderVisibleTogglesVisibility) {
  web_view_ = std::make_unique<ContextualTasksWebView>(browser_window_.get());

  web_view_->SetGhostLoaderVisible(true);
  EXPECT_TRUE(web_view_->IsGhostLoaderVisible());

  web_view_->SetGhostLoaderVisible(false);
  EXPECT_FALSE(web_view_->IsGhostLoaderVisible());
}

TEST_F(ContextualTasksWebViewTest,
       NavigationLifecycleShowsAndHidesGhostLoader) {
  web_view_ = std::make_unique<ContextualTasksWebView>(browser_window_.get());

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile_, nullptr);
  web_view_->SetWebContents(web_contents.get());

  EXPECT_FALSE(web_view_->IsGhostLoaderVisible());

  // Navigation to search results starts: ghost loader should show.
  auto sim = content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://www.google.com/search?q=test"),
      web_contents->GetPrimaryMainFrame());
  sim->Start();

  EXPECT_TRUE(web_view_->IsGhostLoaderVisible());

  // First visually non-empty paint triggers: ghost loader should hide.
  web_view_->DidFirstVisuallyNonEmptyPaint();
  EXPECT_FALSE(web_view_->IsGhostLoaderVisible());
}

TEST_F(ContextualTasksWebViewTest, NavigationToAiPageDoesNotShowGhostLoader) {
  web_view_ = std::make_unique<ContextualTasksWebView>(browser_window_.get());

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile_, nullptr);
  web_view_->SetWebContents(web_contents.get());

  // Navigation to an AI URL starts: ghost loader should NOT show.
  auto sim = content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://ai.google.com/search?q=test"),
      web_contents->GetPrimaryMainFrame());
  sim->Start();

  EXPECT_FALSE(web_view_->IsGhostLoaderVisible());
}

TEST_F(ContextualTasksWebViewTest, FailedNavigationHidesGhostLoader) {
  web_view_ = std::make_unique<ContextualTasksWebView>(browser_window_.get());

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile_, nullptr);
  web_view_->SetWebContents(web_contents.get());

  auto sim = content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://www.google.com/search?q=test"),
      web_contents->GetPrimaryMainFrame());
  sim->Start();
  EXPECT_TRUE(web_view_->IsGhostLoaderVisible());

  // Navigation fails to commit.
  sim->Fail(net::ERR_ABORTED);
  EXPECT_FALSE(web_view_->IsGhostLoaderVisible());
}

TEST_F(ContextualTasksWebViewTest, DidStopLoadingHidesGhostLoader) {
  web_view_ = std::make_unique<ContextualTasksWebView>(browser_window_.get());

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile_, nullptr);
  web_view_->SetWebContents(web_contents.get());

  web_view_->SetGhostLoaderVisible(true);
  EXPECT_TRUE(web_view_->IsGhostLoaderVisible());

  web_view_->DidStopLoading();
  EXPECT_FALSE(web_view_->IsGhostLoaderVisible());
}

TEST_F(ContextualTasksWebViewTest,
       SetWebContentsWithAlreadyLoadingContentsShowsGhostLoader) {
  web_view_ = std::make_unique<ContextualTasksWebView>(browser_window_.get());

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile_, nullptr);
  auto sim = content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://www.google.com/search?q=test"),
      web_contents->GetPrimaryMainFrame());
  sim->Start();

  EXPECT_TRUE(web_contents->IsLoading());

  // Attaching an already-loading WebContents should immediately show the ghost
  // loader.
  web_view_->SetWebContents(web_contents.get());
  EXPECT_TRUE(web_view_->IsGhostLoaderVisible());

  // First paint dismisses the ghost loader.
  web_view_->DidFirstVisuallyNonEmptyPaint();
  EXPECT_FALSE(web_view_->IsGhostLoaderVisible());
}

TEST_F(ContextualTasksWebViewTest,
       SetWebContentsWithAlreadyLoadingAiUrlDoesNotShowGhostLoader) {
  web_view_ = std::make_unique<ContextualTasksWebView>(browser_window_.get());

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile_, nullptr);
  auto sim = content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://ai.google.com/search?q=test"),
      web_contents->GetPrimaryMainFrame());
  sim->Start();

  EXPECT_TRUE(web_contents->IsLoading());

  web_view_->SetWebContents(web_contents.get());
  EXPECT_FALSE(web_view_->IsGhostLoaderVisible());
}

TEST_F(ContextualTasksWebViewTest,
       SetWebContentsWaitingForUrlShowsGhostLoader) {
  web_view_ = std::make_unique<ContextualTasksWebView>(browser_window_.get());

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile_, nullptr);
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  ContextualSearchWebContentsHelper::CreateForWebContents(web_contents.get());
  auto* helper =
      ContextualSearchWebContentsHelper::FromWebContents(web_contents.get());
  helper->SetTaskSession(task_id, nullptr, nullptr);

  auto* ui_service =
      ContextualTasksUiServiceFactory::GetForBrowserContext(profile_);
  ui_service->AddPendingUrlCallback(task_id, base::NullCallback());

  web_view_->SetWebContents(web_contents.get());
  EXPECT_TRUE(web_view_->IsGhostLoaderVisible());

  // DidStopLoading while waiting for URL does not prematurely dismiss loader.
  web_view_->DidStopLoading();
  EXPECT_TRUE(web_view_->IsGhostLoaderVisible());

  // When actual navigation starts and paints, ghost loader dismisses.
  auto sim = content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://www.google.com/search?q=test"),
      web_contents->GetPrimaryMainFrame());
  sim->Start();
  EXPECT_TRUE(web_view_->IsGhostLoaderVisible());

  web_view_->DidFirstVisuallyNonEmptyPaint();
  EXPECT_FALSE(web_view_->IsGhostLoaderVisible());
}

TEST_F(ContextualTasksWebViewTest, RedirectToAiUrlHidesGhostLoader) {
  web_view_ = std::make_unique<ContextualTasksWebView>(browser_window_.get());

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile_, nullptr);
  web_view_->SetWebContents(web_contents.get());

  auto sim = content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://www.google.com/search?q=test"),
      web_contents->GetPrimaryMainFrame());
  sim->Start();
  EXPECT_TRUE(web_view_->IsGhostLoaderVisible());

  // Redirect to an AI URL: ghost loader should be hidden.
  sim->Redirect(GURL("https://ai.google.com/search?q=test"));
  EXPECT_FALSE(web_view_->IsGhostLoaderVisible());
}

TEST_F(ContextualTasksWebViewTest, RedirectToSearchUrlShowsGhostLoader) {
  web_view_ = std::make_unique<ContextualTasksWebView>(browser_window_.get());

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile_, nullptr);
  web_view_->SetWebContents(web_contents.get());

  auto sim = content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://ai.google.com/search?q=test"),
      web_contents->GetPrimaryMainFrame());
  sim->Start();
  EXPECT_FALSE(web_view_->IsGhostLoaderVisible());

  // Redirect to a search URL: ghost loader should be shown.
  sim->Redirect(GURL("https://www.google.com/search?q=test"));
  EXPECT_TRUE(web_view_->IsGhostLoaderVisible());
}

class TestFileSelectListener : public content::FileSelectListener {
 public:
  explicit TestFileSelectListener(base::OnceClosure on_canceled)
      : on_canceled_(std::move(on_canceled)) {}

  // content::FileSelectListener:
  void FileSelected(std::vector<blink::mojom::FileChooserFileInfoPtr> files,
                    const base::FilePath& base_dir,
                    blink::mojom::FileChooserParams::Mode mode) override {}
  void FileSelectionCanceled() override {
    canceled_ = true;
    if (on_canceled_) {
      std::move(on_canceled_).Run();
    }
  }

  bool canceled() const { return canceled_; }

 private:
  ~TestFileSelectListener() override = default;

  base::OnceClosure on_canceled_;
  bool canceled_ = false;
};

// Web content in the side panel must be able to show the file picker, e.g. for
// the "Add images" and "Add files" actions of the AI Mode input plate.
TEST_F(ContextualTasksWebViewTest, RunFileChooserOpensFileDialog) {
  base::RunLoop run_loop;
  ui::FakeSelectFileDialog::Factory* factory =
      ui::FakeSelectFileDialog::RegisterFactory();
  factory->SetOpenCallback(run_loop.QuitClosure());
  base::ScopedClosureRunner reset_factory(
      base::BindOnce([] { ui::SelectFileDialog::SetFactory(nullptr); }));

  web_view_ = std::make_unique<ContextualTasksWebView>(browser_window_.get());
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile_, nullptr);
  web_view_->SetWebContents(web_contents.get());
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents.get(), GURL("https://www.google.com/search?q=test"));

  auto listener =
      base::MakeRefCounted<TestFileSelectListener>(run_loop.QuitClosure());
  blink::mojom::FileChooserParams params;
  params.mode = blink::mojom::FileChooserParams::Mode::kOpen;
  web_contents->GetDelegate()->RunFileChooser(
      web_contents->GetPrimaryMainFrame(), listener, params);
  run_loop.Run();

  EXPECT_FALSE(listener->canceled());
  ui::FakeSelectFileDialog* dialog = factory->GetLastDialog();
  ASSERT_NE(dialog, nullptr);

  // Close the dialog so that `FileSelectHelper` releases its self-reference.
  dialog->CallFileSelectionCanceled();
  EXPECT_TRUE(listener->canceled());
}

}  // namespace
}  // namespace contextual_tasks
