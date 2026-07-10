// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/contextual_cueing/features.h"
#include "chrome/browser/indigo/fake_api.h"
#include "chrome/browser/indigo/indigo_prefs.h"
#include "chrome/browser/indigo/indigo_service.h"
#include "chrome/browser/indigo/indigo_service_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/indigo/indigo_toolbar.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/page_action/anchored_message_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/view.h"

namespace indigo {
namespace {

constexpr std::string_view kStubScript = R"(
  const agent = {
    invoke: function() {
      const img = document.getElementById('target_image');
      if (img) {
        window.indigo.startImageReplacement(img, {disposition: 'primary'});
      } else {
        console.error('Target image not found');
      }
    }
  };
  window.indigo.setup(agent);
)";

constexpr std::string_view kHtmlBody =
    "<!DOCTYPE html>\n"
    "<html><body>\n"
    "<img id=\"target_image\"\n"
    "     src=\"data:image/png;base64,"
    "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk+M"
    "9QDwADhgGAWjR9awAAAABJRU5ErkJggg==\"\n"
    "     style=\"width:200px; height:200px; position:absolute; left:50px; "
    "top:50px;\">\n"
    "</body></html>";

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsId);

class IndigoInteractiveUiTest : public InteractiveBrowserTest {
 public:
  IndigoInteractiveUiTest() = default;
  ~IndigoInteractiveUiTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(fake_api_.InitializeAndListen());
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kIndigo,
          {{features::kIndigoGenerateUrl.name,
            fake_api_.GetGenerateUrl().spec()},
           {features::kIndigoSkipEnterpriseCheck.name, "true"}}},
         {blink::features::kImageReplacement, {}},
         {contextual_cueing::kContextualCueingV2, {}}},
        {});
    InteractiveBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InteractiveBrowserTest::SetUpCommandLine(command_line);
    CHECK(temp_dir_.CreateUniqueTempDir());
    base::FilePath script_path =
        temp_dir_.GetPath().AppendASCII("stub_script.js");
    CHECK(base::WriteFile(script_path, kStubScript));
    command_line->AppendSwitchASCII("indigo-script",
                                    script_path.AsUTF8Unsafe());
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    InteractiveBrowserTest::SetUpOnMainThread();

    IndigoService* service =
        IndigoServiceFactory::GetForProfile(browser()->profile());
    service->SetRemoteEligibilityFetcherForTesting(base::BindRepeating(
        [](IndigoService::RemoteEligibilityCallback callback) {
          std::move(callback).Run(
              RemoteEligibility{.is_service_supported_for_account = true,
                                .has_user_image = true});
        }));

    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
            browser()->profile());
    identity_test_env_adaptor_->identity_test_env()
        ->SetAutomaticIssueOfAccessTokens(true);
    AccountInfo account_info =
        identity_test_env_adaptor_->identity_test_env()
            ->MakePrimaryAccountAvailable("user@example.com",
                                          signin::ConsentLevel::kSignin);
    AccountCapabilitiesTestMutator mutator(&account_info);
    mutator.set_can_use_model_execution_features(true);
    signin::UpdateAccountInfoForAccount(
        identity_test_env_adaptor_->identity_test_env()->identity_manager(),
        account_info);

    browser()->profile()->GetPrefs()->SetBoolean(prefs::kIndigoHasOnboarded,
                                                 true);
    fake_api_.StartAcceptingConnectionsAutomatic();

    embedded_test_server()->RegisterRequestHandler(base::BindLambdaForTesting(
        [&](const net::test_server::HttpRequest& request)
            -> std::unique_ptr<net::test_server::HttpResponse> {
          if (request.relative_url == "/image.html") {
            auto response =
                std::make_unique<net::test_server::BasicHttpResponse>();
            response->set_code(net::HTTP_OK);
            response->set_content(kHtmlBody);
            response->set_content_type("text/html");
            return response;
          }
          return nullptr;
        }));

    ASSERT_TRUE(embedded_test_server()->Start());

    auto* optimization_guide_keyed_service =
        OptimizationGuideKeyedServiceFactory::GetForProfile(
            browser()->profile());
    ASSERT_TRUE(optimization_guide_keyed_service);
    optimization_guide_keyed_service->AddHintForTesting(
        embedded_test_server()->GetURL("/image.html"),
        optimization_guide::proto::OptimizationType::INDIGO, std::nullopt);
  }

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    InteractiveBrowserTest::SetUpBrowserContextKeyedServices(context);
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
  }

  void TearDownOnMainThread() override {
    identity_test_env_adaptor_.reset();
    InteractiveBrowserTest::TearDownOnMainThread();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  FakeApi fake_api_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  base::ScopedTempDir temp_dir_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(IndigoInteractiveUiTest, ToolbarKeyboardFocus) {
  const GURL url = embedded_test_server()->GetURL("/image.html");

  RunTestSequence(
      InstrumentTab(kWebContentsId), NavigateWebContents(kWebContentsId, url),
      WaitForShow(kIndigoPageActionIconElementId),
      WaitForShow(
          page_actions::AnchoredMessageBubbleView::kAnchoredMessageChipId),
      PressButton(
          page_actions::AnchoredMessageBubbleView::kAnchoredMessageChipId),
      WaitForShow(IndigoToolbar::kToolbarElementId),
      WaitForShow(IndigoToolbar::kExpandButtonElementId),

      PressButton(IndigoToolbar::kExpandButtonElementId),
      WaitForShow(IndigoToolbar::kRegenerateButtonElementId),

      WithView(IndigoToolbar::kExpandButtonElementId,
               [this](views::View* expand_button) {
                 views::FocusManager* focus_manager =
                     expand_button->GetFocusManager();
                 ASSERT_NE(focus_manager, nullptr);
                 EXPECT_TRUE(expand_button->IsFocusable());

                 views::View* toolbar_view =
                     BrowserElementsViews::From(browser())->GetView(
                         IndigoToolbar::kToolbarElementId);
                 ASSERT_NE(toolbar_view, nullptr);

                 auto get_view = [&](ui::ElementIdentifier id) {
                   views::View* view = toolbar_view->GetViewByElementId(id);
                   EXPECT_NE(view, nullptr) << "Missing view: " << id.GetName();
                   return view;
                 };

                 // Verify clean entry from preceding external view in window
                 // focus chain.
                 views::View* prev = focus_manager->GetNextFocusableView(
                     expand_button, nullptr, /*reverse=*/true,
                     /*dont_loop=*/false);
                 ASSERT_NE(prev, nullptr);
                 EXPECT_FALSE(toolbar_view->Contains(prev))
                     << "Previous focusable view must be outside the entire "
                        "IndigoToolbar hierarchy.";
                 EXPECT_EQ(focus_manager->GetNextFocusableView(
                               prev, nullptr, /*reverse=*/false,
                               /*dont_loop=*/false),
                           expand_button);

                 // Verify inner toolbar button focus traversal order.
                 std::vector<views::View*> forward_order = {
                     get_view(IndigoToolbar::kCloseButtonElementId),
                     get_view(IndigoToolbar::kRegenerateButtonElementId),
                     get_view(IndigoToolbar::kReplacePhotoButtonElementId),
                     get_view(IndigoToolbar::kDeletePhotoButtonElementId),
                 };
                 views::View* curr = expand_button;
                 for (views::View* expected_view : forward_order) {
                   curr = focus_manager->GetNextFocusableView(
                       curr, nullptr, /*reverse=*/false, /*dont_loop=*/false);
                   EXPECT_EQ(curr, expected_view);
                 }
                 for (int i = static_cast<int>(forward_order.size()) - 2;
                      i >= 0; --i) {
                   curr = focus_manager->GetNextFocusableView(
                       curr, nullptr, /*reverse=*/true, /*dont_loop=*/false);
                   EXPECT_EQ(curr, forward_order[i]);
                 }
                 curr = focus_manager->GetNextFocusableView(
                     curr, nullptr, /*reverse=*/true, /*dont_loop=*/false);
                 EXPECT_EQ(curr, expand_button);

                 // Verify clean exit from toolbar back into window focus chain.
                 views::View* last_button = forward_order.back();
                 views::View* next_external =
                     focus_manager->GetNextFocusableView(last_button, nullptr,
                                                         /*reverse=*/false,
                                                         /*dont_loop=*/false);
                 ASSERT_NE(next_external, nullptr);
                 EXPECT_FALSE(toolbar_view->Contains(next_external))
                     << "Next focusable view must completely exit the "
                        "IndigoToolbar hierarchy.";
                 EXPECT_EQ(focus_manager->GetNextFocusableView(
                               next_external, nullptr, /*reverse=*/true,
                               /*dont_loop=*/false),
                           last_button);
               }),

      PressButton(IndigoToolbar::kCloseButtonElementId),
      WaitForHide(IndigoToolbar::kToolbarElementId));
}

}  // namespace indigo
