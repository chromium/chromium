// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_interactive_test_base.h"

#include <utility>

#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/strings/escape.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_cookie_synchronizer.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_ui_service_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/search_test_utils.h"
#include "components/contextual_tasks/public/features.h"
#include "components/contextual_tasks/public/host_override.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_overlay_permission_utils.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "media/base/media_switches.h"
#include "net/base/url_util.h"
#include "net/dns/mock_host_resolver.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/lens_server_proto/lens_overlay_cluster_info.pb.h"
#include "third_party/lens_server_proto/lens_overlay_server.pb.h"

namespace contextual_tasks {

SkColor TestTabContextualizationController::screenshot_color_ = SK_ColorRED;

TestTabContextualizationController::TestTabContextualizationController(
    tabs::TabInterface* tab)
    : lens::TabContextualizationController(tab) {}

TestTabContextualizationController::~TestTabContextualizationController() =
    default;

void TestTabContextualizationController::CaptureScreenshot(
    std::optional<lens::ImageEncodingOptions> image_options,
    CaptureScreenshotCallback callback) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(100, 100, /*isOpaque=*/true);
  bitmap.eraseColor(screenshot_color_);
  std::move(callback).Run(bitmap);
}

bool TestTabContextualizationController::IsPageContextEligible(
    const GURL& url,
    const std::vector<optimization_guide::FrameMetadata>& frame_metadata) {
  return true;
}

TestContextualTasksEligibilityManager::TestContextualTasksEligibilityManager(
    PrefService* pref_service,
    signin::IdentityManager* identity_manager,
    AimEligibilityService* aim_eligibility_service,
    bool is_signed_in)
    : ContextualTasksEligibilityManager(pref_service,
                                        identity_manager,
                                        aim_eligibility_service),
      is_signed_in_(is_signed_in) {
  MaybeNotifyEligibilityChanged();
}

TestContextualTasksEligibilityManager::
    ~TestContextualTasksEligibilityManager() = default;

bool TestContextualTasksEligibilityManager::IsEligibleWithoutIdentity() const {
  return true;
}

bool TestContextualTasksEligibilityManager::CalculateEligibility() const {
  return is_signed_in_;
}

TestContextualTasksUiService::TestContextualTasksUiService(
    Profile* profile,
    ContextualTasksService* contextual_tasks_service,
    AimEligibilityService* aim_eligibility_service,
    signin::IdentityManager* identity_manager,
    bool is_signed_in)
    : ContextualTasksUiService(
          profile,
          std::make_unique<testing::NiceMock<
              contextual_tasks::MockContextualTasksUiServiceDelegate>>(),
          contextual_tasks_service,
          identity_manager,
          aim_eligibility_service,
          std::make_unique<TestContextualTasksEligibilityManager>(
              profile->GetPrefs(),
              identity_manager,
              aim_eligibility_service,
              is_signed_in),
          /*cookie_synchronizer=*/nullptr),
      is_signed_in_(is_signed_in) {}

TestContextualTasksUiService::~TestContextualTasksUiService() = default;

bool TestContextualTasksUiService::IsSignedInToBrowserWithValidCredentials() {
  return is_signed_in_;
}

bool TestContextualTasksUiService::CookieJarContainsPrimaryAccount() {
  return is_signed_in_;
}

bool TestContextualTasksUiService::IsUrlForPrimaryAccount(const GURL& url) {
  return is_signed_in_;
}

void TestContextualTasksUiService::SetSignedIn(bool is_signed_in) {
  is_signed_in_ = is_signed_in;
}

void TestContextualTasksUiService::GetAccessToken(
    GetAccessTokenCallback callback,
    base::WeakPtr<content::WebContents> web_contents) {
  if (is_signed_in_) {
    std::move(callback).Run("fake_access_token");
  } else {
    std::move(callback).Run("");
  }
}

ContextualTasksInteractiveTestBase::~ContextualTasksInteractiveTestBase() =
    default;

void ContextualTasksInteractiveTestBase::InitTabContextOverride() {
  tab_context_override_ =
      tabs::TabFeatures::GetUserDataFactoryForTesting()
          .AddOverrideForTesting<lens::TabContextualizationController>(
              base::BindRepeating(
                  [](tabs::TabInterface& tab)
                      -> std::unique_ptr<lens::TabContextualizationController> {
                    return std::make_unique<TestTabContextualizationController>(
                        &tab);
                  }));
}

UserVariation ContextualTasksInteractiveTestBase::GetUserVariation() const {
  return UserVariation::kSignedIn;
}

void ContextualTasksInteractiveTestBase::SetUpCommandLine(
    base::CommandLine* command_line) {
  LensOverlayInteractiveTestBase::SetUpCommandLine(command_line);
  if (GetUserVariation() == UserVariation::kIncognito) {
    command_line->AppendSwitch(::switches::kIncognito);
  }
}

// static
std::vector<base::test::FeatureRefAndParams>
ContextualTasksInteractiveTestBase::GetDefaultEnabledFeatures() {
  return {
      {kContextualTasks, {}},
      {lens::features::kLensOverlay, {}},
      {lens::features::kLensSidePanelUnification,
       {{"allow-signed-out", "true"}}},
      {lens::features::kLensOverlayContextualSearchbox,
       {{"use-pdfs-as-context", "true"}, {"auto-focus-searchbox", "false"}}},
      {lens::features::kLensOverlayTranslateButton, {}},
      {media::kContextMenuSearchForVideoFrame, {}},
  };
}

// static
std::vector<base::test::FeatureRef>
ContextualTasksInteractiveTestBase::GetDefaultDisabledFeatures() {
  return {
      lens::features::kLensSendRawFileMediaTypes,
      omnibox::internal::kWebUIOmniboxPopup,
      omnibox::internal::kWebUIOmniboxAimPopup,
      features::kNonBlockingOsClipboardReads,
  };
}

void ContextualTasksInteractiveTestBase::SetUpFeatureList() {
  feature_list_.InitWithFeaturesAndParameters(GetDefaultEnabledFeatures(),
                                              GetDefaultDisabledFeatures());
}

void ContextualTasksInteractiveTestBase::SetUpBrowserContextKeyedServices(
    content::BrowserContext* context) {
  LensOverlayInteractiveTestBase::SetUpBrowserContextKeyedServices(context);

  IdentityTestEnvironmentProfileAdaptor::
      SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);

  AimEligibilityServiceFactory::GetInstance()->SetTestingFactory(
      context,
      base::BindRepeating(
          &ContextualTasksInteractiveTestBase::BuildMockAimServiceInstance,
          base::Unretained(this)));

  ContextualTasksUiServiceFactory::GetInstance()->SetTestingFactory(
      context,
      base::BindRepeating(&ContextualTasksInteractiveTestBase::
                              BuildMockContextualTasksUiServiceInstance,
                          base::Unretained(this)));
}

void ContextualTasksInteractiveTestBase::SetUpOnMainThread() {
  TestTabContextualizationController::screenshot_color_ = SK_ColorRED;
  LensOverlayInteractiveTestBase::SetUpOnMainThread();

  if (GetUserVariation() == UserVariation::kSignedIn) {
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
            browser()->GetProfile());
    identity_test_env_adaptor_->identity_test_env()
        ->MakePrimaryAccountAvailable("user@example.com",
                                      signin::ConsentLevel::kSignin);
    identity_test_env_adaptor_->identity_test_env()
        ->SetAutomaticIssueOfAccessTokens(true);
  }

  browser()->GetProfile()->GetPrefs()->SetBoolean(
      lens::prefs::kLensSharingPageScreenshotEnabled, true);
  browser()->GetProfile()->GetPrefs()->SetBoolean(
      lens::prefs::kLensSharingPageContentEnabled, true);

  host_resolver()->AddRule("*", "127.0.0.1");

  contextual_tasks::SetForcedEmbeddedPageHostOverride(
      contextual_tasks::HostOverride{kMockAimPageHost});

  url_loader_interceptor_ = std::make_unique<content::URLLoaderInterceptor>(
      base::BindLambdaForTesting(
          [&](content::URLLoaderInterceptor::RequestParams* params) {
            const GURL& url = params->url_request.url;
            if (url.host() == kMockAimPageHost &&
                (url.path() == "/complete/s" ||
                 url.path() == "/complete/search")) {
              std::string q_param;
              net::GetValueForKeyInQuery(url, "q", &q_param);
              std::string query = base::UnescapeURLComponent(
                  q_param, base::UnescapeRule::REPLACE_PLUS_WITH_SPACE);
              std::string response_json = base::StringPrintf(
                  ")]}'\n"
                  R"(["%s", ["suggestion-1", "suggestion-2"]])",
                  query.c_str());
              content::URLLoaderInterceptor::WriteResponse(
                  "HTTP/1.1 200 OK\nContent-Type: application/json\n\n",
                  response_json, params->client.get());
              return true;
            }
            if (url.host() == "a.google.com") {
              content::URLLoaderInterceptor::WriteResponse(
                  "HTTP/1.1 200 OK\nContent-Type: text/html\n\n",
                  "<html><body>Title 1</body></html>", params->client.get());
              return true;
            }
            if (url.host() == kMockAimPageHost) {
              content::URLLoaderInterceptor::WriteResponse(
                  kMockAimPagePath, params->client.get());
              return true;
            }
            GURL cluster_info_url{
                lens::features::GetLensOverlayClusterInfoEndpointUrl()};
            GURL upload_url{lens::features::GetLensOverlayEndpointURL()};
            GURL composebox_cluster_info_url{
                lens::features::GetLensComposeboxClusterInfoEndpointUrl()};
            GURL composebox_upload_url{
                lens::features::GetLensComposeboxEndpointUrl()};
            if ((url.host() == cluster_info_url.host() &&
                 url.path() == cluster_info_url.path()) ||
                (url.host() == composebox_cluster_info_url.host() &&
                 url.path() == composebox_cluster_info_url.path())) {
              lens::LensOverlayServerClusterInfoResponse response;
              response.set_search_session_id("test_search_session_id");
              std::string response_string;
              CHECK(response.SerializeToString(&response_string));
              content::URLLoaderInterceptor::WriteResponse(
                  "HTTP/1.1 200 OK\nContent-Type: application/x-protobuf\n\n",
                  response_string, params->client.get());
              return true;
            }
            if ((url.host() == upload_url.host() &&
                 url.path() == upload_url.path()) ||
                (url.host() == composebox_upload_url.host() &&
                 url.path() == composebox_upload_url.path())) {
              lens::LensOverlayServerResponse response;
              std::string response_string;
              CHECK(response.SerializeToString(&response_string));
              content::URLLoaderInterceptor::WriteResponse(
                  "HTTP/1.1 200 OK\nContent-Type: application/x-protobuf\n\n",
                  response_string, params->client.get());
              return true;
            }
            return false;
          }));
}

void ContextualTasksInteractiveTestBase::TearDownOnMainThread() {
  identity_test_env_adaptor_.reset();
  url_loader_interceptor_.reset();
  contextual_tasks::SetForcedEmbeddedPageHostOverride(std::nullopt);
  LensOverlayInteractiveTestBase::TearDownOnMainThread();
}

std::unique_ptr<KeyedService>
ContextualTasksInteractiveTestBase::BuildMockAimServiceInstance(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  auto mock = std::make_unique<MockAimEligibilityService>(
      CHECK_DEREF(profile->GetPrefs()), /*template_url_service=*/nullptr,
      /*url_loader_factory=*/nullptr,
      IdentityManagerFactory::GetForProfile(profile));

  auto* config = &mock->config();
  config->add_input_type_configs()->set_input_type(
      omnibox::INPUT_TYPE_BROWSER_TAB);
  config->add_input_type_configs()->set_input_type(
      omnibox::INPUT_TYPE_LENS_IMAGE);
  config->add_input_type_configs()->set_input_type(
      omnibox::INPUT_TYPE_LENS_FILE);

  ON_CALL(*mock, GetSearchboxConfig()).WillByDefault(testing::Return(config));

  ON_CALL(*mock, IsAimUrl(testing::_, testing::_))
      .WillByDefault(
          [](const GURL& url,
             std::optional<contextual_tasks::HostOverride> host_override) {
            if (url.path() == "/search") {
              std::string udm;
              if (net::GetValueForKeyInQuery(url, "udm", &udm) && udm == "50") {
                return true;
              }
              std::string q;
              if (net::GetValueForKeyInQuery(url, "q", &q) && q == "thread") {
                return true;
              }
              return false;
            }
            return url.host().find(kMockAimPageHost) != std::string::npos ||
                   url.host().find("g.ai") != std::string::npos;
          });

  ON_CALL(*mock, IsAimHost(testing::_, testing::_))
      .WillByDefault(
          [](const GURL& url,
             std::optional<contextual_tasks::HostOverride> host_override) {
            return url.host().find(kMockAimPageHost) != std::string::npos ||
                   url.host().find("g.ai") != std::string::npos;
          });

  ON_CALL(*mock, HasAimUrlParams(testing::_))
      .WillByDefault(testing::Return(true));
  ON_CALL(*mock, IsCobrowseEligible()).WillByDefault(testing::Return(true));
  ON_CALL(*mock, IsAimEligible()).WillByDefault(testing::Return(true));
  ON_CALL(*mock, RegisterEligibilityChangedCallback(testing::_))
      .WillByDefault([](base::RepeatingClosure) {
        return base::CallbackListSubscription();
      });

  return mock;
}

std::unique_ptr<KeyedService>
ContextualTasksInteractiveTestBase::BuildMockContextualTasksUiServiceInstance(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  const bool is_signed_in = (GetUserVariation() == UserVariation::kSignedIn);
  return std::make_unique<TestContextualTasksUiService>(
      profile, ContextualTasksServiceFactory::GetForProfile(profile),
      AimEligibilityServiceFactory::GetForProfile(profile),
      IdentityManagerFactory::GetForProfile(profile), is_signed_in);
}

MockAimEligibilityService*
ContextualTasksInteractiveTestBase::GetMockAimEligibilityService(
    Profile* profile) {
  auto* service = AimEligibilityServiceFactory::GetForProfile(profile);
  return static_cast<MockAimEligibilityService*>(service);
}

TestContextualTasksUiService*
ContextualTasksInteractiveTestBase::GetTestContextualTasksUiService(
    Profile* profile) {
  auto* service =
      ContextualTasksUiServiceFactory::GetForBrowserContext(profile);
  return static_cast<TestContextualTasksUiService*>(service);
}

}  // namespace contextual_tasks
