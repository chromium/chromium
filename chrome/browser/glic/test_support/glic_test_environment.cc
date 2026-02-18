// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/test_support/glic_test_environment.h"

#include "base/command_line.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "chrome/browser/glic/fre/glic_fre_controller.h"
#include "chrome/browser/glic/host/auth_controller.h"
#include "chrome/browser/glic/host/glic_cookie_synchronizer.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/common/chrome_switches.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/url_util.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/constants/chromeos_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#include "components/signin/public/android/jni_headers/AccountCapabilities_jni.h"
#include "components/signin/public/android/jni_headers/AccountInfo_jni.h"
#pragma clang diagnostic pop

namespace {

// Defined locally to avoid changes to identity_test_utils.h/cc.
void AddTestAccountToFakeAccountManagerFacade(const std::string& email,
                                              const std::string& gaia_id) {
  JNIEnv* env = base::android::AttachCurrentThread();

  // 1. Construct GaiaId
  GaiaId gaia_id_cpp(gaia_id);

  // 2. Construct CoreAccountId
  CoreAccountId core_account_id_cpp = CoreAccountId::FromGaiaId(gaia_id_cpp);

  // 3. Construct AccountCapabilities (empty)
  std::vector<std::string> cap_names;
  std::vector<bool> cap_values;
  base::android::ScopedJavaLocalRef<jobjectArray> j_cap_names =
      base::android::ToJavaArrayOfStrings(env, cap_names);
  base::android::ScopedJavaLocalRef<jbooleanArray> j_cap_values =
      base::android::ToJavaBooleanArray(env, cap_values);
  base::android::ScopedJavaLocalRef<jobject> account_capabilities_obj =
      signin::Java_AccountCapabilities_Constructor(env, j_cap_names,
                                                   j_cap_values);

  // 4. Construct AccountInfo
  base::android::ScopedJavaLocalRef<jobject> account_info_obj =
      signin::Java_AccountInfo_Constructor(
          env, core_account_id_cpp, email, gaia_id_cpp, "Test User", "Test",
          base::android::ScopedJavaLocalRef<jstring>(),  // hostedDomain
          base::android::ScopedJavaLocalRef<jobject>(),  // accountImage
          account_capabilities_obj);

  // 5. Get AccountManagerFacadeProvider class and getInstance method
  base::android::ScopedJavaLocalRef<jclass> provider_class =
      base::android::GetClass(
          env, "org/chromium/components/signin/AccountManagerFacadeProvider");
  jmethodID get_instance_method =
      base::android::MethodID::Get<base::android::MethodID::TYPE_STATIC>(
          env, provider_class.obj(), "getInstance",
          "()Lorg/chromium/components/signin/AccountManagerFacade;");

  // 6. Get AccountManagerFacade instance
  base::android::ScopedJavaLocalRef<jobject> facade_obj =
      base::android::ScopedJavaLocalRef<jobject>::Adopt(
          env, env->CallStaticObjectMethod(provider_class.obj(),
                                           get_instance_method));

  // 7. Get addAccount method (FakeAccountManagerFacade specific)
  base::android::ScopedJavaLocalRef<jclass> facade_class =
      base::android::ScopedJavaLocalRef<jclass>::Adopt(
          env, env->GetObjectClass(facade_obj.obj()));
  jmethodID add_account_method =
      base::android::MethodID::Get<base::android::MethodID::TYPE_INSTANCE>(
          env, facade_class.obj(), "addAccount",
          "(Lorg/chromium/components/signin/base/AccountInfo;)V");

  // 8. Call addAccount
  env->CallVoidMethod(facade_obj.obj(), add_account_method,
                      account_info_obj.obj());
}

}  // namespace
#endif

namespace glic {

namespace internal {
GlicTestEnvironmentConfig& GetConfig() {
  static GlicTestEnvironmentConfig config;
  return config;
}

// A fake GlicCookieSynchronizer.
class TestCookieSynchronizer : public glic::GlicCookieSynchronizer {
 public:
  static std::pair<TestCookieSynchronizer*, TestCookieSynchronizer*>
  InjectForProfile(Profile* profile) {
    GlicKeyedService* service =
        GlicKeyedServiceFactory::GetGlicKeyedService(profile, true);
    auto cookie_synchronizer = std::make_unique<TestCookieSynchronizer>(
        profile, IdentityManagerFactory::GetForProfile(profile),
        /*for_fre=*/false);
    TestCookieSynchronizer* ptr = cookie_synchronizer.get();
    service->GetAuthController().SetCookieSynchronizerForTesting(
        std::move(cookie_synchronizer));

    auto fre_cookie_synchronizer = std::make_unique<TestCookieSynchronizer>(
        profile, IdentityManagerFactory::GetForProfile(profile),
        /*for_fre=*/true);
    TestCookieSynchronizer* fre_cookie_synchronizer_ptr =
        fre_cookie_synchronizer.get();
    service->fre_controller()
        .GetAuthControllerForTesting()
        .SetCookieSynchronizerForTesting(std::move(fre_cookie_synchronizer));

    return std::make_pair(ptr, fre_cookie_synchronizer_ptr);
  }

  using GlicCookieSynchronizer::GlicCookieSynchronizer;

  void CopyCookiesToWebviewStoragePartition(
      base::OnceCallback<void(bool)> callback) override {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), copy_cookies_result_));
  }

  void set_copy_cookies_result(bool result) { copy_cookies_result_ = result; }
  base::WeakPtr<TestCookieSynchronizer> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  bool copy_cookies_result_ = true;

  base::WeakPtrFactory<TestCookieSynchronizer> weak_ptr_factory_{this};
};

class GlicTestEnvironmentServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static GlicTestEnvironmentService* GetForProfile(Profile* profile,
                                                   bool create) {
    return static_cast<GlicTestEnvironmentService*>(
        GetInstance()->GetServiceForBrowserContext(profile, create));
  }
  static GlicTestEnvironmentServiceFactory* GetInstance() {
    static base::NoDestructor<GlicTestEnvironmentServiceFactory> instance;
    return instance.get();
  }

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override {
    return std::make_unique<GlicTestEnvironmentService>(
        Profile::FromBrowserContext(context));
  }

 private:
  friend class base::NoDestructor<GlicTestEnvironmentServiceFactory>;

  GlicTestEnvironmentServiceFactory()
      : ProfileKeyedServiceFactory(
            "GlicTestEnvironmentService",
            ProfileSelections::BuildForRegularProfile()) {
    DependsOn(IdentityManagerFactory::GetInstance());
    // It would be sensible to depend on GlicKeyedServiceFactory, but that ends
    // up creating some service factories too early.
  }
  ~GlicTestEnvironmentServiceFactory() override = default;
};

}  // namespace internal

std::vector<base::test::FeatureRef> GetDefaultEnabledGlicTestFeatures() {
  return {features::kGlic, features::kGlicRollout,
#if BUILDFLAG(IS_CHROMEOS)
          chromeos::features::kFeatureManagementGlic
#endif  // BUILDFLAG(IS_CHROMEOS)
  };
}
std::vector<base::test::FeatureRef> GetDefaultDisabledGlicTestFeatures() {
  return {features::kGlicWarming, features::kGlicFreWarming,
          features::kGlicCountryFiltering, features::kGlicLocaleFiltering};
}

GlicTestEnvironment::GlicTestEnvironment(
    const GlicTestEnvironmentConfig& config,
    std::vector<base::test::FeatureRef> enabled_features,
    std::vector<base::test::FeatureRef> disabled_features)
    : shared_(enabled_features, disabled_features) {
  internal::GetConfig() = config;

  // The service factory needs to be created before any services are created.
  internal::GlicTestEnvironmentServiceFactory::GetInstance();
  create_services_subscription_ =
      BrowserContextDependencyManager::GetInstance()
          ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
              &GlicTestEnvironment::OnWillCreateBrowserContextKeyedServices,
              base::Unretained(this)));

#if BUILDFLAG(IS_ANDROID)
  // On Android, SigninManager validates the account with the OS AccountManager
  // (mediated by AccountManagerFacade). We must set up the fake facade.
  // The account seeding is done in OnWillCreateBrowserContextKeyedServices
  // to run after TestingProfile::Init resets the facade.
  signin::SetUpFakeAccountManagerFacade();
#endif
}

GlicTestEnvironment::~GlicTestEnvironment() = default;

void GlicTestEnvironment::SetForceSigninAndModelExecutionCapability(
    bool force) {
  internal::GetConfig().force_signin_and_glic_capability = force;
}

void GlicTestEnvironment::SetFreStatusForNewProfiles(
    std::optional<prefs::FreStatus> fre_status) {
  internal::GetConfig().fre_status = fre_status;
}

GlicTestEnvironmentService* GlicTestEnvironment::GetService(Profile* profile,
                                                            bool create) {
  return internal::GlicTestEnvironmentServiceFactory::GetForProfile(profile,
                                                                    create);
}

void GlicTestEnvironmentService::SetModelExecutionCapability(bool enabled) {
  ::glic::SetGlicCapability(profile_, enabled);
}

void GlicTestEnvironment::OnWillCreateBrowserContextKeyedServices(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  if (!profile || !GlicEnabling::IsProfileEligible(profile)) {
    LOG(WARNING) << "Not creating GlicTestEnvironmentService for "
                    "ineligible profile.";
    return;
  }
  if (internal::GetConfig().force_signin_and_glic_capability) {
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);

    adaptor_ = std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile);
  }

  auto observation =
      std::make_unique<base::ScopedObservation<Profile, ProfileObserver>>(this);
  observation->Observe(profile);
  profile_observations_.push_back(std::move(observation));
}

void GlicTestEnvironment::OnProfileWillBeDestroyed(Profile* profile) {
  std::erase_if(profile_observations_, [&profile](const auto& observation) {
    return observation->GetSource() == profile;
  });
}

void GlicTestEnvironment::OnProfileInitializationComplete(Profile* profile) {
  GetService(profile, true);
}

bool GlicTestEnvironment::SetupEmbeddedTestServers(
    net::test_server::EmbeddedTestServer* http_server,
    net::test_server::EmbeddedTestServer* https_server) {
  CHECK(guest_url_.is_empty()) << "SetupEmbeddedTestServers called twice";

  http_server->ServeFilesFromDirectory(
      base::PathService::CheckedGet(base::DIR_ASSETS)
          .AppendASCII("gen/chrome/test/data/webui/glic/"));
  http_server->ServeFilesFromSourceDirectory("chrome/test/data/webui/glic/");
  if (https_server) {
    https_server->ServeFilesFromDirectory(
        base::PathService::CheckedGet(base::DIR_ASSETS)
            .AppendASCII("gen/chrome/test/data/webui/glic/"));
    https_server->ServeFilesFromSourceDirectory("chrome/test/data/webui/glic/");
  }

  test_server_handle_ = http_server->StartAndReturnHandle();
  if (!test_server_handle_) {
    return false;
  }

  // Need to set this here rather than in SetUpCommandLine because we need to
  // use the embedded test server to get the right URL and it's not started
  // at that time.
  std::ostringstream path;
  path << glic_page_path_;

  // Append the query parameters to the URL.
  bool first_param = true;
  for (const auto& [key, value] : mock_glic_query_params_) {
    path << (first_param ? "?" : "&");
    first_param = false;
    path << url::EncodeUriComponent(key);
    if (!value.empty()) {
      path << "=" << url::EncodeUriComponent(value);
    }
  }

  auto* command_line = base::CommandLine::ForCurrentProcess();
  guest_url_ = http_server->GetURL(path.str());
  command_line->AppendSwitchASCII(::switches::kGlicGuestURL, guest_url_.spec());

  if (glic_fre_url_override_) {
    glic_fre_url_ = *glic_fre_url_override_;
  } else {
    glic_fre_url_ = http_server->GetURL("/glic/test_client/fre.html");
  }
  command_line->AppendSwitchASCII(switches::kGlicFreURL, glic_fre_url_->spec());

  return true;
}

void GlicTestEnvironment::SetGlicPagePath(const std::string& path) {
  CHECK(guest_url_.is_empty())
      << "SetGlicPagePath must be called before SetupEmbeddedTestServers";
  glic_page_path_ = path;
}

void GlicTestEnvironment::AddMockGlicQueryParam(const std::string_view& key,
                                                const std::string_view& value) {
  CHECK(guest_url_.is_empty())
      << "AddMockGlicQueryParam must be called before SetupEmbeddedTestServers";
  mock_glic_query_params_.emplace(key, value);
}

void GlicTestEnvironment::SetGlicFreUrlOverride(const GURL& url) {
  CHECK(guest_url_.is_empty())
      << "SetGlicFreUrlOverride must be called before SetupEmbeddedTestServers";
  glic_fre_url_override_ = url;
}

GURL GlicTestEnvironment::GetGuestURL() const {
  CHECK(guest_url_.is_valid()) << "Guest URL not yet configured.";
  return guest_url_;
}

const std::optional<GURL>& GlicTestEnvironment::GetGlicFreUrl() const {
  CHECK(glic_fre_url_.has_value()) << "GLIC FRE URL not yet configured.";
  return glic_fre_url_;
}

GlicTestEnvironmentService::GlicTestEnvironmentService(Profile* profile)
    : profile_(profile) {
  std::pair<internal::TestCookieSynchronizer*,
            internal::TestCookieSynchronizer*>
      cookie_synchronizers =
          internal::TestCookieSynchronizer::InjectForProfile(profile);

  cookie_synchronizer_ = cookie_synchronizers.first->GetWeakPtr();
  fre_cookie_synchronizer_ = cookie_synchronizers.second->GetWeakPtr();
  const GlicTestEnvironmentConfig& config = internal::GetConfig();
  if (config.fre_status) {
    SetFRECompletion(*config.fre_status);
  }
  if (config.force_signin_and_glic_capability) {
#if BUILDFLAG(IS_CHROMEOS)
    // SigninWithPrimaryAccount below internally runs RunLoop to wait for an
    // async task completion. This is the test only behavior.
    // However, that has side effect that other tasks in the queue also runs.
    // Specifically, some of the queued tasks will require communicating with
    // SigninBrowserContext, and if it's not existing, the instance is created.
    // Because this running inside a KeyedService construction,
    // KeyedServiceTemplatedFactory temporarily keeps the map entry between
    // the current browser context and the info of this KeyedService instance.
    // However, the entry is invalidated if another BrowserContext is created,
    // like SigninBrowserContext case explained above, regardless of whether
    // the newly created BrowserContext will create this service or not.
    // Thus, after returning from this function, it will cause crashes.
    // To avoid such crashes, while running the async task, we temporarily
    // disable the creation of new BrowserContext for the workaround.
    // See also crbug.com/460334478 for details.
    auto disabled = ash::BrowserContextHelper::
        DisableImplicitBrowserContextCreationForTest();
#endif
#if BUILDFLAG(IS_ANDROID)
    // Seed the account into the FakeAccountManagerFacade.
    // This is required on Android because SigninManager checks the facade
    // directly.
    AddTestAccountToFakeAccountManagerFacade(
        "glic-test@example.com",
        signin::GetTestGaiaIdForEmail("glic-test@example.com").ToString());
#endif
    SigninWithPrimaryAccount(profile);
    SetModelExecutionCapability(true);
  }
}

GlicTestEnvironmentService::~GlicTestEnvironmentService() = default;

GlicKeyedService* GlicTestEnvironmentService::GetService() {
  return GlicKeyedServiceFactory::GetGlicKeyedService(profile_);
}

void GlicTestEnvironmentService::SetResultForFutureCookieSync(bool result) {
  cookie_synchronizer_->set_copy_cookies_result(result);
}

void GlicTestEnvironmentService::SetResultForFutureCookieSyncInFre(
    bool result) {
  fre_cookie_synchronizer_->set_copy_cookies_result(result);
}

void GlicTestEnvironmentService::SetFRECompletion(prefs::FreStatus fre_status) {
  ::glic::SetFRECompletion(profile_, fre_status);
}

GlicUnitTestEnvironment::GlicUnitTestEnvironment(
    const GlicTestEnvironmentConfig& config)
    : config_(config),
      shared_(GetDefaultEnabledGlicTestFeatures(),
              GetDefaultDisabledGlicTestFeatures()) {}

GlicUnitTestEnvironment::~GlicUnitTestEnvironment() = default;

void GlicUnitTestEnvironment::SetupProfile(Profile* profile) {
  if (config_.force_signin_and_glic_capability) {
    ::glic::ForceSigninAndGlicCapability(profile);
  }
  if (config_.fre_status) {
    glic::SetFRECompletion(profile, *config_.fre_status);
  }
}

internal::GlicTestEnvironmentShared::GlicTestEnvironmentShared(
    std::vector<base::test::FeatureRef> enabled_features,
    std::vector<base::test::FeatureRef> disabled_features) {
  scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
}

}  // namespace glic
