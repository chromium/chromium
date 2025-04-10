// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/linux_mac_windows/supervised_user_web_content_handler_impl.h"

#include <memory>
#include <optional>
#include <string>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/field_trial_params.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/supervised_user/linux_mac_windows/parent_access_dialog_result_observer.h"
#include "chrome/browser/supervised_user/linux_mac_windows/parent_access_view.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/supervised_user/supervision_mixin.h"
#include "components/supervised_user/core/browser/proto/parent_access_callback.pb.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/supervised_user/test_support/parent_access_test_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/base/net_errors.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace {
constexpr char kPacpHost[] = "families.google.com";

enum class ResponseBehaviour : int {
  kHttpOk = 0,
  kHttpRedirection = 1,
};

class SupervisedUserWebContentHandlerImplTest
    : public MixinBasedInProcessBrowserTest {
 public:
  SupervisedUserWebContentHandlerImplTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{supervised_user::kLocalWebApprovals},
        /*disabled_features=*/{});
  }

  content::WebContents* contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  supervised_user::SupervisionMixin& supervision_mixin() {
    return supervision_mixin_;
  }

  supervised_user::SupervisedUserURLFilter* GetUrlFilter() {
    Profile* profile =
        Profile::FromBrowserContext(contents()->GetBrowserContext());
    supervised_user::SupervisedUserService* supervised_user_service =
        SupervisedUserServiceFactory::GetForProfile(profile);
    supervised_user::SupervisedUserURLFilter* url_filter =
        supervised_user_service->GetURLFilter();
    CHECK(url_filter);
    return url_filter;
  }

  void OverrideResponseBehaviour(ResponseBehaviour behaviour) {
    http_behaviour_ = behaviour;
  }

  content::WebContents* MockParentApprovalDialogNavigationToUrlWithResult(
      SupervisedUserWebContentHandlerImpl* handler,
      std::string url_base64_encoded_result) {
    // Constructs the url containing the PACP result as a query param
    // (`result`).
    supervision_mixin()
        .api_mock_setup_mixin()
        .api_mock()
        .AllowSubsequentClassifyUrl();
    GURL::Replacements result_query_param;
    std::string param_value = "result=" + url_base64_encoded_result;
    result_query_param.SetQueryStr(param_value);
    OverrideResponseBehaviour(ResponseBehaviour::kHttpRedirection);
    GURL pacp_origin_url_with_result =
        embedded_test_server()
            ->GetURL(kPacpHost, "/")
            .ReplaceComponents(result_query_param);

    // Makes the parent access dialog navigate to the url that contains the PACP
    // result. This causes the dialog to close and destruct.
    auto* dialog_contents = handler->GetObserverContentsForTesting();
    CHECK(dialog_contents);
    content::NavigationController& controller =
        dialog_contents->GetController();
    content::NavigationController::LoadURLParams params(
        pacp_origin_url_with_result);
    params.transition_type = ui::PAGE_TRANSITION_LINK;
    controller.LoadURLWithParams(params);
    return dialog_contents;
  }

 private:
  void SetUp() override {
    // Mock the responses to navigations via the test server.
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &SupervisedUserWebContentHandlerImplTest::HandleRedirection,
        base::Unretained(this)));
    MixinBasedInProcessBrowserTest::SetUp();
  }

  // TODO(crbug.com/387261453): Also used in
  // parent_access_dialog_web_contents_observer_browsertest.cc. The tests have
  // similar setup and mocks. The common parts could be split into a helper or
  // a base class.
  std::unique_ptr<net::test_server::HttpResponse> HandleRedirection(
      const net::test_server::HttpRequest& request) {
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();

    switch (http_behaviour_) {
      case ResponseBehaviour::kHttpRedirection:
        //  Mimics the last url in a seriers of PACP re-directions.
        response->set_code(net::HTTP_MOVED_PERMANENTLY);
        response->AddCustomHeader("Location",
                                  supervised_user::kFamilyManagementUrl);
        break;
      case ResponseBehaviour::kHttpOk:
        response->set_code(net::HTTP_OK);
        break;
    }
    return response;
  }

  supervised_user::SupervisionMixin supervision_mixin_{
      mixin_host_,
      this,
      embedded_test_server(),
      {.sign_in_mode =
           supervised_user::SupervisionMixin::SignInMode::kSupervised,
       .embedded_test_server_options = {.resolver_rules_map_host_list =
                                            kPacpHost}}};
  base::test::ScopedFeatureList feature_list_;
  ResponseBehaviour http_behaviour_ = ResponseBehaviour::kHttpOk;
};

IN_PROC_BROWSER_TEST_F(SupervisedUserWebContentHandlerImplTest,
                       ObservePacpNavigationAndRecordApprovalResult) {
  base::HistogramTester histogram_tester;

  CHECK(contents());
  auto handler = std::make_unique<SupervisedUserWebContentHandlerImpl>(
      contents(), content::FrameTreeNodeId(), 0);

  supervised_user::UrlFormatter url_formatter(
      *GetUrlFilter(), supervised_user::FilteringBehaviorReason::DEFAULT);
  GURL blocked_url("https://www.example.com/");
  // Makes a local approval request and checks that the PACP dialog is created.
  handler->RequestLocalApproval(
      blocked_url, u"child_display_name", url_formatter,
      supervised_user::FilteringBehaviorReason::MANUAL,
      /*callback*/ base::DoNothing());
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return handler->GetWeakParentAccessViewForTesting() != nullptr;
  }));

  // Mocks the web contents dialog observer obtaining a PACP response:
  // We get the PACP dialog contents and use them to navigate to a url
  // that contains the PACP response.
  // The request handler `HandleRedirection` mocks the re-direction to the
  // `pacp_end_url` reached by PACP, in order to complete the approval flow.
  GURL pacp_end_url = GURL(supervised_user::kFamilyManagementUrl);
  auto observer =
      std::make_unique<content::TestNavigationObserver>(pacp_end_url);

  content::WebContents* dialog_contents =
      MockParentApprovalDialogNavigationToUrlWithResult(
          handler.get(), supervised_user::CreatePacpApprovalResult());
  CHECK(dialog_contents);

  // Ensure the parent access dialog reached the `pacp_end_url`, before checking
  // the local approval metrics.
  observer->WatchWebContents(dialog_contents);
  observer->WaitForNavigationFinished();
  observer.reset();

  histogram_tester.ExpectBucketCount(
      supervised_user::kLocalWebApprovalResultHistogramName,
      static_cast<int>(supervised_user::LocalApprovalResult::kApproved), 1);
  histogram_tester.ExpectTotalCount(
      supervised_user::kLocalWebApprovalResultHistogramName, 1);
  histogram_tester.ExpectTotalCount(
      supervised_user::kLocalWebApprovalErrorTypeHistogramName, 0);
  // Duration metrics are recorded for the approval case.
  histogram_tester.ExpectTotalCount(
      "FamilyLinkUser.LocalWebApprovalCompleteRequestTotalDuration", 1);

  // Check that the dialog has been destructed (after it was closed).
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return handler->GetWeakParentAccessViewForTesting() == nullptr;
  }));
}

IN_PROC_BROWSER_TEST_F(SupervisedUserWebContentHandlerImplTest,
                       ObservePacpNavigationAndRecordInvalidResult) {
  base::HistogramTester histogram_tester;

  CHECK(contents());
  auto handler = std::make_unique<SupervisedUserWebContentHandlerImpl>(
      contents(), content::FrameTreeNodeId(), 0);

  supervised_user::UrlFormatter url_formatter(
      *GetUrlFilter(), supervised_user::FilteringBehaviorReason::DEFAULT);
  GURL blocked_url("https://www.example.com/");
  // Makes a local approval request and checks that the PACP dialog is created.
  handler->RequestLocalApproval(
      blocked_url, u"child_display_name", url_formatter,
      supervised_user::FilteringBehaviorReason::MANUAL,
      /*callback*/ base::DoNothing());
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return handler->GetWeakParentAccessViewForTesting() != nullptr;
  }));

  // Mocks the web contents dialog observer obtaining a PACP response:
  // We get the PACP dialog contents and use them to navigate to a url
  // that contains the PACP response.
  // The request handler `HandleRedirection` mocks the re-direction to the
  // `pacp_end_url` reached by PACP, in order to complete the approval flow.
  GURL pacp_end_url = GURL(supervised_user::kFamilyManagementUrl);
  auto observer =
      std::make_unique<content::TestNavigationObserver>(pacp_end_url);

  content::WebContents* dialog_contents =
      MockParentApprovalDialogNavigationToUrlWithResult(
          handler.get(), base::Base64Encode("invalid_response"));
  CHECK(dialog_contents);

  // Ensure the parent access dialog reached the `pacp_end_url`, before checking
  // the local approval metrics.
  observer->WatchWebContents(dialog_contents);
  observer->WaitForNavigationFinished();
  observer.reset();

  histogram_tester.ExpectBucketCount(
      supervised_user::kLocalWebApprovalResultHistogramName,
      static_cast<int>(supervised_user::LocalApprovalResult::kError), 1);
  histogram_tester.ExpectTotalCount(
      supervised_user::kLocalWebApprovalResultHistogramName, 1);
  histogram_tester.ExpectBucketCount(
      supervised_user::kLocalWebApprovalErrorTypeHistogramName,
      static_cast<int>(supervised_user::LocalWebApprovalErrorType::
                           kFailureToParsePacpResponse),
      1);
  histogram_tester.ExpectTotalCount(
      supervised_user::kLocalWebApprovalErrorTypeHistogramName, 1);

  if (base::FeatureList::IsEnabled(
          supervised_user::kEnableLocalWebApprovalErrorDialog)) {
    // Check that the dialog content was replaced with the error message
    // content.
    EXPECT_TRUE(base::test::RunUntil([&]() {
      return handler->GetWeakParentAccessViewForTesting()
                     ->GetErrorViewForTesting() != nullptr &&
             handler->GetWeakParentAccessViewForTesting()
                     ->GetWebViewForTesting() == nullptr;
    }));
  } else {
    // Check that the PACP dialog is destructed.
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return handler->GetWeakParentAccessViewForTesting() == nullptr;
    }));
  }
}

IN_PROC_BROWSER_TEST_F(SupervisedUserWebContentHandlerImplTest,
                       ParentApprovalNotInitiatedWhenHostEmpty) {
  base::HistogramTester histogram_tester;

  CHECK(contents());
  auto handler = std::make_unique<SupervisedUserWebContentHandlerImpl>(
      contents(), content::FrameTreeNodeId(), 0);

  supervised_user::UrlFormatter url_formatter(
      *GetUrlFilter(), supervised_user::FilteringBehaviorReason::DEFAULT);
  GURL blocked_invalid_url;

  bool approval_initiated;
  auto approval_initiated_lambda = [](bool& result,
                                      bool actual_approval_initiated) {
    result = actual_approval_initiated;
  };

  // Makes a local approval request and checks that the PACP dialog is created.
  handler->RequestLocalApproval(
      blocked_invalid_url, u"child_display_name", url_formatter,
      supervised_user::FilteringBehaviorReason::MANUAL,
      /*callback=*/
      base::BindOnce(approval_initiated_lambda, std::ref(approval_initiated)));
  EXPECT_EQ(false, approval_initiated);
}

IN_PROC_BROWSER_TEST_F(SupervisedUserWebContentHandlerImplTest,
                       UniqueDialogWhenLocalApprovalRequestInProgress) {
  base::HistogramTester histogram_tester;

  CHECK(contents());
  auto handler = std::make_unique<SupervisedUserWebContentHandlerImpl>(
      contents(), content::FrameTreeNodeId(), 0);

  supervised_user::UrlFormatter url_formatter(
      *GetUrlFilter(), supervised_user::FilteringBehaviorReason::DEFAULT);
  GURL blocked_url("https://www.example.com/");

  bool approval_initiated;
  auto approval_initiated_lambda = [](bool& result,
                                      bool actual_approval_initiated) {
    result = actual_approval_initiated;
  };

  // Makes a local approval request and checks that the PACP dialog is created.
  handler->RequestLocalApproval(
      blocked_url, u"child_display_name", url_formatter,
      supervised_user::FilteringBehaviorReason::MANUAL,
      /*callback=*/
      base::BindOnce(approval_initiated_lambda, std::ref(approval_initiated)));
  EXPECT_EQ(true, approval_initiated);

  // Make another approval request, which should return early while another is
  // in progress.
  handler->RequestLocalApproval(
      blocked_url, u"child_display_name", url_formatter,
      supervised_user::FilteringBehaviorReason::MANUAL,
      /*callback=*/
      base::BindOnce(approval_initiated_lambda, std::ref(approval_initiated)));
  EXPECT_EQ(false, approval_initiated);

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return handler->GetWeakParentAccessViewForTesting() != nullptr;
  }));
  // Close the parent approval dialog.
  views::Widget* widget =
      handler->GetWeakParentAccessViewForTesting()->GetWidget();
  ASSERT_TRUE(widget);
  widget->CloseWithReason(views::Widget::ClosedReason::kCloseButtonClicked);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return handler->GetWeakParentAccessViewForTesting() == nullptr;
  }));

  // The next local approval request should go through.
  handler->RequestLocalApproval(
      blocked_url, u"child_display_name", url_formatter,
      supervised_user::FilteringBehaviorReason::MANUAL,
      /*callback=*/
      base::BindOnce(approval_initiated_lambda, std::ref(approval_initiated)));
  EXPECT_EQ(true, approval_initiated);
}

IN_PROC_BROWSER_TEST_F(SupervisedUserWebContentHandlerImplTest,
                       RecordDialogCancellationOnCloseButton) {
  base::HistogramTester histogram_tester;

  CHECK(contents());
  auto handler = std::make_unique<SupervisedUserWebContentHandlerImpl>(
      contents(), content::FrameTreeNodeId(), 0);

  supervised_user::UrlFormatter url_formatter(
      *GetUrlFilter(), supervised_user::FilteringBehaviorReason::DEFAULT);
  GURL blocked_url("https://www.example.com/");
  // Makes a local approval request and checks that the PACP dialog is created.
  handler->RequestLocalApproval(
      blocked_url, u"child_display_name", url_formatter,
      supervised_user::FilteringBehaviorReason::MANUAL,
      /*callback*/ base::DoNothing());
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return handler->GetWeakParentAccessViewForTesting() != nullptr;
  }));

  // Mimic closing the dialog via the Close button.
  views::Widget* widget =
      handler->GetWeakParentAccessViewForTesting()->GetWidget();
  ASSERT_TRUE(widget);
  widget->CloseWithReason(views::Widget::ClosedReason::kCloseButtonClicked);

  // Check that the dialog has been destructed (after it was closed).
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return handler->GetWeakParentAccessViewForTesting() == nullptr;
  }));

  histogram_tester.ExpectBucketCount(
      supervised_user::kLocalWebApprovalResultHistogramName,
      static_cast<int>(supervised_user::LocalApprovalResult::kCanceled), 1);
  histogram_tester.ExpectTotalCount(
      supervised_user::kLocalWebApprovalResultHistogramName, 1);
  histogram_tester.ExpectTotalCount(
      supervised_user::kLocalWebApprovalErrorTypeHistogramName, 0);
}

IN_PROC_BROWSER_TEST_F(SupervisedUserWebContentHandlerImplTest,
                       RecordDialogCancellationOnInterstitialDestruction) {
  base::HistogramTester histogram_tester;

  CHECK(contents());
  auto handler = std::make_unique<SupervisedUserWebContentHandlerImpl>(
      contents(), content::FrameTreeNodeId(), 0);

  supervised_user::UrlFormatter url_formatter(
      *GetUrlFilter(), supervised_user::FilteringBehaviorReason::DEFAULT);
  GURL blocked_url("https://www.example.com/");
  // Makes a local approval request and checks that the PACP dialog is created.
  handler->RequestLocalApproval(
      blocked_url, u"child_display_name", url_formatter,
      supervised_user::FilteringBehaviorReason::MANUAL,
      /*callback*/ base::DoNothing());
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return handler->GetWeakParentAccessViewForTesting() != nullptr;
  }));

  // This method is called by the interstitial destructor.
  handler->MaybeCloseLocalApproval();

  // Check that the dialog has been destructed (after it was closed).
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return handler->GetWeakParentAccessViewForTesting() == nullptr;
  }));

  histogram_tester.ExpectBucketCount(
      supervised_user::kLocalWebApprovalResultHistogramName,
      static_cast<int>(supervised_user::LocalApprovalResult::kCanceled), 1);
  histogram_tester.ExpectTotalCount(
      supervised_user::kLocalWebApprovalResultHistogramName, 1);
  histogram_tester.ExpectTotalCount(
      supervised_user::kLocalWebApprovalErrorTypeHistogramName, 0);
}

class SupervisedUserParentAccessViewWithTimeoutTest
    : public SupervisedUserWebContentHandlerImplTest {
 public:
  SupervisedUserParentAccessViewWithTimeoutTest() {
    // Override PACP timeout to 10 ms.
    int timeout_ms = 10;
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{supervised_user::kLocalWebApprovals,
          {{supervised_user::kLocalWebApprovalBottomSheetLoadTimeoutMs.name,
            base::NumberToString(timeout_ms)}}}},
        {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SupervisedUserWebContentHandlerImplTest,
                       RecordDialogErrorOnPacpTimeout) {
  base::HistogramTester histogram_tester;

  CHECK(contents());
  auto handler = std::make_unique<SupervisedUserWebContentHandlerImpl>(
      contents(), content::FrameTreeNodeId(), 0);

  supervised_user::UrlFormatter url_formatter(
      *GetUrlFilter(), supervised_user::FilteringBehaviorReason::DEFAULT);
  GURL blocked_url("https://www.example.com/");
  // Makes a local approval request and checks that the PACP dialog is created.
  handler->RequestLocalApproval(
      blocked_url, u"child_display_name", url_formatter,
      supervised_user::FilteringBehaviorReason::MANUAL,
      /*callback*/ base::DoNothing());
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return handler->GetWeakParentAccessViewForTesting() != nullptr &&
           handler->GetWeakParentAccessViewForTesting()
                   ->GetWebViewForTesting() != nullptr &&
           !handler->GetWeakParentAccessViewForTesting()
                ->GetWebViewForTesting()
                ->GetVisible();
  }));

  if (base::FeatureList::IsEnabled(
          supervised_user::kEnableLocalWebApprovalErrorDialog)) {
    // Check that the PACP dialog content is replaced by the error message
    // content.
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return handler->GetWeakParentAccessViewForTesting()
                     ->GetWebViewForTesting() == nullptr &&
             (base::FeatureList::IsEnabled(
                  supervised_user::kEnableLocalWebApprovalErrorDialog)
                  ? handler->GetWeakParentAccessViewForTesting()
                            ->GetErrorViewForTesting() != nullptr
                  : true);
    }));
  } else {
    // Check that the PACP dialog is destructed.
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return handler->GetWeakParentAccessViewForTesting() == nullptr;
    }));
  }

  histogram_tester.ExpectBucketCount(
      supervised_user::kLocalWebApprovalResultHistogramName,
      static_cast<int>(supervised_user::LocalApprovalResult::kError), 1);
  histogram_tester.ExpectTotalCount(
      supervised_user::kLocalWebApprovalResultHistogramName, 1);
  histogram_tester.ExpectBucketCount(
      supervised_user::kLocalWebApprovalErrorTypeHistogramName,
      static_cast<int>(
          supervised_user::LocalWebApprovalErrorType::kPacpTimeoutExceeded),
      1);
  histogram_tester.ExpectTotalCount(
      supervised_user::kLocalWebApprovalErrorTypeHistogramName, 1);
}

class SupervisedUserParentAccessViewErrorScreenUiTest
    : public InteractiveBrowserTestT<SupervisedUserWebContentHandlerImplTest> {
 public:
  SupervisedUserParentAccessViewErrorScreenUiTest() {
    // Override PACP timeout to 0 ms.
    int timeout_ms = 0;
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{supervised_user::kEnableLocalWebApprovalErrorDialog, {}},
         {supervised_user::kLocalWebApprovals,
          {{supervised_user::kLocalWebApprovalBottomSheetLoadTimeoutMs.name,
            base::NumberToString(timeout_ms)}}}},
        {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SupervisedUserParentAccessViewErrorScreenUiTest,
                       ShowAndCloseErrorScreen) {
  CHECK(contents());
  auto handler = std::make_unique<SupervisedUserWebContentHandlerImpl>(
      contents(), content::FrameTreeNodeId(), 0);

  supervised_user::UrlFormatter url_formatter(
      *GetUrlFilter(), supervised_user::FilteringBehaviorReason::DEFAULT);
  GURL blocked_url("https://www.example.com/");

  // Makes a local approval request that times out immediately
  // and checks that the error dialog is shown and can be dismissed by the
  // "Back" button.
  RunTestSequence(InAnyContext(
      Do([&handler, &url_formatter, &blocked_url]() -> void {
        handler->RequestLocalApproval(
            blocked_url, u"child_display_name", url_formatter,
            supervised_user::FilteringBehaviorReason::MANUAL,
            /*callback=*/ base::DoNothing());
      }),
      WaitForShow(kLocalWebParentApprovalDialogErrorId),
      WaitForShow(ParentAccessView::kErrorDialogBackButtonElementId),
      PressButton(ParentAccessView::kErrorDialogBackButtonElementId),
      WaitForHide(kLocalWebParentApprovalDialogErrorId)));
}

}  // namespace
