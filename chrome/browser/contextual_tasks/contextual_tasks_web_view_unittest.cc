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
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/contextual_tasks/public/features.h"
#include "components/lens/lens_overlay_dismissal_source.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/render_process_host.h"
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

using testing::_;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;

namespace contextual_tasks {
namespace {

class MockLensSearchController : public LensSearchController {
 public:
  explicit MockLensSearchController(tabs::TabInterface* tab)
      : LensSearchController(tab) {}
  ~MockLensSearchController() override = default;

  using LensSearchController::CloseLensAsync;
  MOCK_METHOD(void,
              CloseLensAsync,
              (lens::LensOverlayDismissalSource dismissal_source),
              (override));
};

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
    return url.GetHost() == "ai.google.com" ||
           url.GetQuery().find("udm=50") != std::string::npos;
  }

  bool IsSearchResultsUrl(const GURL& url) override {
    return url.GetHost() == "www.google.com" && url.GetPath() == "/search";
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

  // DidStopLoading and DidFirstVisuallyNonEmptyPaint while waiting for URL
  // do not prematurely dismiss loader.
  web_view_->DidStopLoading();
  EXPECT_TRUE(web_view_->IsGhostLoaderVisible());
  web_view_->DidFirstVisuallyNonEmptyPaint();
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

TEST_F(ContextualTasksWebViewTest,
       AboutBlankNavigationAndPaintDoNotHideGhostLoader) {
  web_view_ = std::make_unique<ContextualTasksWebView>(browser_window_.get());

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile_, nullptr);
  web_view_->SetWebContents(web_contents.get());
  web_view_->SetGhostLoaderVisible(true);
  EXPECT_TRUE(web_view_->IsGhostLoaderVisible());

  // An aborted about:blank navigation should not hide the ghost loader.
  auto aborted_blank_sim = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("about:blank"), web_contents.get());
  aborted_blank_sim->Start();
  aborted_blank_sim->AbortCommit();
  EXPECT_TRUE(web_view_->IsGhostLoaderVisible());

  // A committed about:blank navigation and its first paint / stop loading
  // should not hide the ghost loader while waiting for the real search page.
  auto committed_blank_sim =
      content::NavigationSimulator::CreateBrowserInitiated(GURL("about:blank"),
                                                           web_contents.get());
  committed_blank_sim->Commit();
  EXPECT_TRUE(web_view_->IsGhostLoaderVisible());

  web_view_->DidFirstVisuallyNonEmptyPaint();
  EXPECT_TRUE(web_view_->IsGhostLoaderVisible());

  web_view_->DidStopLoading();
  EXPECT_TRUE(web_view_->IsGhostLoaderVisible());

  // Once the real search navigation commits and paints, the ghost loader hides.
  auto search_sim = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://www.google.com/search?q=test"), web_contents.get());
  search_sim->SetKeepLoading(true);
  search_sim->Start();
  EXPECT_TRUE(web_view_->IsGhostLoaderVisible());

  // Late paint while about:blank is still the committed URL does not hide it.
  web_view_->DidFirstVisuallyNonEmptyPaint();
  EXPECT_TRUE(web_view_->IsGhostLoaderVisible());

  search_sim->Commit();
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

TEST_F(ContextualTasksWebViewTest, TransitionFromSrpToAiPageClosesLens) {
  tabs::MockTabInterface mock_tab;
  NiceMock<MockLensSearchController> mock_lens_controller(&mock_tab);
  ON_CALL(*browser_window_, GetActiveTabInterface())
      .WillByDefault(Return(&mock_tab));

  web_view_ = std::make_unique<ContextualTasksWebView>(browser_window_.get());
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile_, nullptr);
  web_view_->SetWebContents(web_contents.get());

  // Navigating to SRP should not close Lens.
  EXPECT_CALL(mock_lens_controller, CloseLensAsync(_)).Times(0);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents.get(), GURL("https://www.google.com/search?q=test"));
  testing::Mock::VerifyAndClearExpectations(&mock_lens_controller);

  // Transitioning from SRP to AIM should close Lens.
  EXPECT_CALL(
      mock_lens_controller,
      CloseLensAsync(
          lens::LensOverlayDismissalSource::kContextualTasksQuerySubmitted))
      .Times(1);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents.get(), GURL("https://ai.google.com/search?q=test"));
}

TEST_F(ContextualTasksWebViewTest,
       SameDocumentTransitionFromSrpToAiPageClosesLens) {
  tabs::MockTabInterface mock_tab;
  NiceMock<MockLensSearchController> mock_lens_controller(&mock_tab);
  ON_CALL(*browser_window_, GetActiveTabInterface())
      .WillByDefault(Return(&mock_tab));

  web_view_ = std::make_unique<ContextualTasksWebView>(browser_window_.get());
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile_, nullptr);
  web_view_->SetWebContents(web_contents.get());

  EXPECT_CALL(mock_lens_controller, CloseLensAsync(_)).Times(0);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents.get(), GURL("https://www.google.com/search?q=test"));
  testing::Mock::VerifyAndClearExpectations(&mock_lens_controller);

  EXPECT_CALL(
      mock_lens_controller,
      CloseLensAsync(
          lens::LensOverlayDismissalSource::kContextualTasksQuerySubmitted))
      .Times(1);
  auto sim = content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://www.google.com/search?q=test&udm=50"),
      web_contents->GetPrimaryMainFrame());
  sim->CommitSameDocument();
}

TEST_F(ContextualTasksWebViewTest, InitialNavigationToAiPageDoesNotCloseLens) {
  tabs::MockTabInterface mock_tab;
  NiceMock<MockLensSearchController> mock_lens_controller(&mock_tab);
  ON_CALL(*browser_window_, GetActiveTabInterface())
      .WillByDefault(Return(&mock_tab));

  web_view_ = std::make_unique<ContextualTasksWebView>(browser_window_.get());
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile_, nullptr);
  web_view_->SetWebContents(web_contents.get());

  EXPECT_CALL(mock_lens_controller, CloseLensAsync(_)).Times(0);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents.get(), GURL("https://ai.google.com/search?q=test"));
}

TEST_F(ContextualTasksWebViewTest,
       SetWebContentsAlreadyOnSrpThenTransitionToAiPageClosesLens) {
  tabs::MockTabInterface mock_tab;
  NiceMock<MockLensSearchController> mock_lens_controller(&mock_tab);
  ON_CALL(*browser_window_, GetActiveTabInterface())
      .WillByDefault(Return(&mock_tab));

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile_, nullptr);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents.get(), GURL("https://www.google.com/search?q=test"));

  web_view_ = std::make_unique<ContextualTasksWebView>(browser_window_.get());
  web_view_->SetWebContents(web_contents.get());

  EXPECT_CALL(
      mock_lens_controller,
      CloseLensAsync(
          lens::LensOverlayDismissalSource::kContextualTasksQuerySubmitted))
      .Times(1);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents.get(), GURL("https://ai.google.com/search?q=test"));
}

TEST_F(ContextualTasksWebViewTest,
       TransitionFromSrpToNonSrpThenAiPageDoesNotCloseLens) {
  tabs::MockTabInterface mock_tab;
  NiceMock<MockLensSearchController> mock_lens_controller(&mock_tab);
  ON_CALL(*browser_window_, GetActiveTabInterface())
      .WillByDefault(Return(&mock_tab));

  web_view_ = std::make_unique<ContextualTasksWebView>(browser_window_.get());
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile_, nullptr);
  web_view_->SetWebContents(web_contents.get());

  EXPECT_CALL(mock_lens_controller, CloseLensAsync(_)).Times(0);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents.get(), GURL("https://www.google.com/search?q=test"));
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents.get(), GURL("https://example.com"));
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents.get(), GURL("https://ai.google.com/search?q=test"));
}

class MockTabWebContentsDelegate : public content::WebContentsDelegate {
 public:
  MOCK_METHOD(content::WebContents*,
              OpenURLFromTab,
              (content::WebContents * source,
               const content::OpenURLParams& params,
               base::OnceCallback<void(content::NavigationHandle&)>
                   navigation_handle_callback),
              (override));
};

TEST_F(ContextualTasksWebViewTest, OpenURLFromTab_RejectsNonWebSchemes) {
  tabs::MockTabInterface mock_tab;
  std::unique_ptr<content::WebContents> tab_contents =
      content::WebContentsTester::CreateTestWebContents(profile_, nullptr);
  NiceMock<MockTabWebContentsDelegate> tab_delegate;
  tab_contents->SetDelegate(&tab_delegate);
  ON_CALL(mock_tab, GetContents()).WillByDefault(Return(tab_contents.get()));
  ON_CALL(*browser_window_, GetActiveTabInterface())
      .WillByDefault(Return(&mock_tab));

  web_view_ = std::make_unique<ContextualTasksWebView>(browser_window_.get());
  std::unique_ptr<content::WebContents> panel_contents =
      content::WebContentsTester::CreateTestWebContents(profile_, nullptr);
  web_view_->SetWebContents(panel_contents.get());

  EXPECT_CALL(tab_delegate, OpenURLFromTab(_, _, _)).Times(0);
  EXPECT_CALL(*browser_window_, OpenURL(_, _)).Times(0);

  const GURL kNonWebUrls[] = {
      GURL("chrome://settings"), GURL("javascript:alert(1)"),
      GURL("data:text/html,hi"), GURL("file:///etc/passwd"),
      GURL("about:blank"),
  };
  for (const GURL& url : kNonWebUrls) {
    content::OpenURLParams params(url, content::Referrer(),
                                  WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                  ui::PAGE_TRANSITION_LINK,
                                  /*is_renderer_initiated=*/true);
    params.user_gesture = true;
    EXPECT_EQ(nullptr, web_view_->OpenURLFromTab(panel_contents.get(), params,
                                                 base::NullCallback()));
  }

  tab_contents->SetDelegate(nullptr);
}

TEST_F(
    ContextualTasksWebViewTest,
    OpenURLFromTab_SanitizesSpoofedGestureAndDispositionAndRoutesThroughActiveTab) {
  tabs::MockTabInterface mock_tab;
  std::unique_ptr<content::WebContents> tab_contents =
      content::WebContentsTester::CreateTestWebContents(profile_, nullptr);
  NiceMock<MockTabWebContentsDelegate> tab_delegate;
  tab_contents->SetDelegate(&tab_delegate);
  ON_CALL(mock_tab, GetContents()).WillByDefault(Return(tab_contents.get()));
  ON_CALL(*browser_window_, GetActiveTabInterface())
      .WillByDefault(Return(&mock_tab));

  web_view_ = std::make_unique<ContextualTasksWebView>(browser_window_.get());
  std::unique_ptr<content::WebContents> panel_contents =
      content::WebContentsTester::CreateTestWebContents(profile_, nullptr);
  web_view_->SetWebContents(panel_contents.get());

  content::RenderFrameHost* panel_rfh = panel_contents->GetPrimaryMainFrame();
  ASSERT_FALSE(panel_rfh->HasTransientUserActivation());

  const GURL kTargetUrl("https://example.com/target");
  content::OpenURLParams params(kTargetUrl, content::Referrer(),
                                WindowOpenDisposition::CURRENT_TAB,
                                ui::PAGE_TRANSITION_LINK,
                                /*is_renderer_initiated=*/true);
  params.user_gesture = true;
  params.source_render_process_id = panel_rfh->GetProcess()->GetDeprecatedID();
  params.source_render_frame_id = panel_rfh->GetRoutingID();
  params.frame_tree_node_id = panel_rfh->GetFrameTreeNodeId();

  EXPECT_CALL(*browser_window_, OpenURL(_, _)).Times(0);
  EXPECT_CALL(tab_delegate, OpenURLFromTab(tab_contents.get(), _, _))
      .WillOnce([&](content::WebContents* source,
                    const content::OpenURLParams& forwarded_params,
                    base::OnceCallback<void(content::NavigationHandle&)>) {
        EXPECT_EQ(kTargetUrl, forwarded_params.url);
        EXPECT_FALSE(forwarded_params.user_gesture);
        EXPECT_EQ(WindowOpenDisposition::NEW_FOREGROUND_TAB,
                  forwarded_params.disposition);
        EXPECT_TRUE(forwarded_params.frame_tree_node_id.is_null());
        return tab_contents.get();
      });

  EXPECT_EQ(tab_contents.get(),
            web_view_->OpenURLFromTab(panel_contents.get(), params,
                                      base::NullCallback()));

  tab_contents->SetDelegate(nullptr);
}

TEST_F(ContextualTasksWebViewTest,
       OpenURLFromTab_PreservesVerifiedUserActivation) {
  tabs::MockTabInterface mock_tab;
  std::unique_ptr<content::WebContents> tab_contents =
      content::WebContentsTester::CreateTestWebContents(profile_, nullptr);
  NiceMock<MockTabWebContentsDelegate> tab_delegate;
  tab_contents->SetDelegate(&tab_delegate);
  ON_CALL(mock_tab, GetContents()).WillByDefault(Return(tab_contents.get()));
  ON_CALL(*browser_window_, GetActiveTabInterface())
      .WillByDefault(Return(&mock_tab));

  web_view_ = std::make_unique<ContextualTasksWebView>(browser_window_.get());
  std::unique_ptr<content::WebContents> panel_contents =
      content::WebContentsTester::CreateTestWebContents(profile_, nullptr);
  web_view_->SetWebContents(panel_contents.get());

  content::RenderFrameHost* panel_rfh = panel_contents->GetPrimaryMainFrame();
  content::RenderFrameHostTester::For(panel_rfh)->SimulateUserActivation();
  ASSERT_TRUE(panel_rfh->HasTransientUserActivation());

  content::OpenURLParams params(
      GURL("https://example.com/target"), content::Referrer(),
      WindowOpenDisposition::NEW_BACKGROUND_TAB, ui::PAGE_TRANSITION_LINK,
      /*is_renderer_initiated=*/true);
  params.user_gesture = true;
  params.source_render_process_id = panel_rfh->GetProcess()->GetDeprecatedID();
  params.source_render_frame_id = panel_rfh->GetRoutingID();

  EXPECT_CALL(tab_delegate, OpenURLFromTab(tab_contents.get(), _, _))
      .WillOnce([&](content::WebContents* source,
                    const content::OpenURLParams& forwarded_params,
                    base::OnceCallback<void(content::NavigationHandle&)>) {
        EXPECT_TRUE(forwarded_params.user_gesture);
        EXPECT_EQ(WindowOpenDisposition::NEW_BACKGROUND_TAB,
                  forwarded_params.disposition);
        return tab_contents.get();
      });

  EXPECT_EQ(tab_contents.get(),
            web_view_->OpenURLFromTab(panel_contents.get(), params,
                                      base::NullCallback()));

  tab_contents->SetDelegate(nullptr);
}

TEST_F(ContextualTasksWebViewTest,
       OpenURLFromTab_RejectsCrossWebContentsInitiatorActivation) {
  tabs::MockTabInterface mock_tab;
  std::unique_ptr<content::WebContents> tab_contents =
      content::WebContentsTester::CreateTestWebContents(profile_, nullptr);
  NiceMock<MockTabWebContentsDelegate> tab_delegate;
  tab_contents->SetDelegate(&tab_delegate);
  ON_CALL(mock_tab, GetContents()).WillByDefault(Return(tab_contents.get()));
  ON_CALL(*browser_window_, GetActiveTabInterface())
      .WillByDefault(Return(&mock_tab));

  web_view_ = std::make_unique<ContextualTasksWebView>(browser_window_.get());
  std::unique_ptr<content::WebContents> panel_contents =
      content::WebContentsTester::CreateTestWebContents(profile_, nullptr);
  web_view_->SetWebContents(panel_contents.get());

  // Activate `tab_contents` (outside the panel) and attempt to spoof its frame
  // ID as the source of a panel OpenURL request.
  content::RenderFrameHostTester::For(tab_contents->GetPrimaryMainFrame())
      ->SimulateUserActivation();
  ASSERT_TRUE(
      tab_contents->GetPrimaryMainFrame()->HasTransientUserActivation());

  content::OpenURLParams params(
      GURL("https://example.com/target"), content::Referrer(),
      WindowOpenDisposition::NEW_FOREGROUND_TAB, ui::PAGE_TRANSITION_LINK,
      /*is_renderer_initiated=*/true);
  params.user_gesture = true;
  params.source_render_process_id =
      tab_contents->GetPrimaryMainFrame()->GetProcess()->GetDeprecatedID();
  params.source_render_frame_id =
      tab_contents->GetPrimaryMainFrame()->GetRoutingID();

  EXPECT_CALL(tab_delegate, OpenURLFromTab(tab_contents.get(), _, _))
      .WillOnce([&](content::WebContents* source,
                    const content::OpenURLParams& forwarded_params,
                    base::OnceCallback<void(content::NavigationHandle&)>) {
        EXPECT_FALSE(forwarded_params.user_gesture);
        return nullptr;
      });

  web_view_->OpenURLFromTab(panel_contents.get(), params, base::NullCallback());

  tab_contents->SetDelegate(nullptr);
}

TEST_F(ContextualTasksWebViewTest,
       OpenURLFromTab_ContextMenuPreservesDispositionAndGesture) {
  tabs::MockTabInterface mock_tab;
  std::unique_ptr<content::WebContents> tab_contents =
      content::WebContentsTester::CreateTestWebContents(profile_, nullptr);
  NiceMock<MockTabWebContentsDelegate> tab_delegate;
  tab_contents->SetDelegate(&tab_delegate);
  ON_CALL(mock_tab, GetContents()).WillByDefault(Return(tab_contents.get()));
  ON_CALL(*browser_window_, GetActiveTabInterface())
      .WillByDefault(Return(&mock_tab));

  web_view_ = std::make_unique<ContextualTasksWebView>(browser_window_.get());
  std::unique_ptr<content::WebContents> panel_contents =
      content::WebContentsTester::CreateTestWebContents(profile_, nullptr);
  web_view_->SetWebContents(panel_contents.get());

  content::OpenURLParams params(
      GURL("https://example.com/incognito"), content::Referrer(),
      WindowOpenDisposition::OFF_THE_RECORD, ui::PAGE_TRANSITION_LINK,
      /*is_renderer_initiated=*/false);
  params.user_gesture = true;
  params.started_from_context_menu = true;

  EXPECT_CALL(tab_delegate, OpenURLFromTab(tab_contents.get(), _, _))
      .WillOnce([&](content::WebContents* source,
                    const content::OpenURLParams& forwarded_params,
                    base::OnceCallback<void(content::NavigationHandle&)>) {
        EXPECT_TRUE(forwarded_params.user_gesture);
        EXPECT_EQ(WindowOpenDisposition::OFF_THE_RECORD,
                  forwarded_params.disposition);
        return tab_contents.get();
      });

  EXPECT_EQ(tab_contents.get(),
            web_view_->OpenURLFromTab(panel_contents.get(), params,
                                      base::NullCallback()));

  tab_contents->SetDelegate(nullptr);
}

}  // namespace
}  // namespace contextual_tasks
