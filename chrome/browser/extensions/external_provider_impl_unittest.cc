// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_provider_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_path_override.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/external_testing_loader.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/web_applications/preinstalled_app_install_features.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_test_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/pref_names.h"
#include "extensions/browser/updater/extension_cache_fake.h"
#include "extensions/browser/updater/extension_downloader_test_helper.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/customization/customization_document.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/user_manager/scoped_user_manager.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#endif

namespace extensions {

namespace {

struct TestServerExtension {
  const char* update_path;
  const char* app_id;
  const char* app_path;
  const char* version;
  const char* crx_path;
};

constexpr const TestServerExtension kInAppPaymentsApp{
    "/update_manifest", extension_misc::kInAppPaymentsSupportAppId,
    "/dummyiap.crx", "1.0.0.4", "extensions/dummyiap.crx"};

constexpr const TestServerExtension kGoodApp{
    "/update_good", "ldnnhddmnhbkjipkidpdiheffobcpfmf", "/good.crx", "1.0.0.0",
    "extensions/good.crx"};

constexpr const TestServerExtension kTestServerExtensions[] = {
    kInAppPaymentsApp,
    kGoodApp,
};

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
const char kExternalAppId[] = "kekdneafjmhmndejhmbcadfiiofngffo";
#endif

#if BUILDFLAG(IS_WIN)
const char kExternalAppCrxPath[] =
    "external\\kekdneafjmhmndejhmbcadfiiofngffo.crx";
const wchar_t kExternalAppRegistryKey[] =
    L"Software\\Google\\Chrome\\Extensions\\kekdneafjmhmndejhmbcadfiiofngffo";
#endif

class ExternalProviderImplTest : public ExtensionServiceTestBase {
 public:
  ExternalProviderImplTest() {}

  ExternalProviderImplTest(const ExternalProviderImplTest&) = delete;
  ExternalProviderImplTest& operator=(const ExternalProviderImplTest&) = delete;

  ~ExternalProviderImplTest() override {}

  void InitService() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    user_manager::ScopedUserManager scoped_user_manager(
        std::make_unique<ash::FakeChromeUserManager>());
#endif
    InitializeExtensionServiceWithUpdaterAndPrefs();

    service()->updater()->SetExtensionCacheForTesting(
        test_extension_cache_.get());

    // Don't install pre-installed apps. Some of the pre-installed apps are
    // downloaded from the webstore, ignoring the url we pass to
    // kAppsGalleryUpdateURL, which would cause the external updates to never
    // finish install.
    profile_->GetPrefs()->SetString(prefs::kPreinstalledApps, "");
  }

  void InitServiceWithExternalProviders(
      const std::optional<bool> block_external = std::nullopt) {
    InitService();

    if (block_external.has_value())
      SetExternalExtensionsBlockedByPolicy(block_external.value());

    // This switch is set when creating a TestingProfile, but needs to be
    // removed for some ExternalProviders to be created.
    base::CommandLine::ForCurrentProcess()->RemoveSwitch(
        switches::kDisableDefaultApps);

    ProviderCollection providers;
    ExternalProviderImpl::CreateExternalProviders(service_, profile_.get(),
                                                  &providers);

    for (std::unique_ptr<ExternalProviderInterface>& provider : providers)
      service_->AddProviderForTesting(std::move(provider));
  }

  void OverrideExternalExtensionsPath() {
    // Windows doesn't use the provider that installs the |kExternalAppId|
    // extension implicitly, so to test that the blocking policy works on
    // Windows it is installed through a Windows-specific registry provider.
#if BUILDFLAG(IS_WIN)
    EXPECT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_CURRENT_USER));
    EXPECT_EQ(ERROR_SUCCESS,
              external_extension_key_.Create(
                  HKEY_CURRENT_USER, kExternalAppRegistryKey, KEY_ALL_ACCESS));
    EXPECT_EQ(ERROR_SUCCESS,
              external_extension_key_.WriteValue(
                  L"path",
                  data_dir().AppendASCII(kExternalAppCrxPath).value().c_str()));
    EXPECT_EQ(ERROR_SUCCESS,
              external_extension_key_.WriteValue(L"version", L"1"));
#else
    external_externsions_overrides_ =
        std::make_unique<base::ScopedPathOverride>(
            chrome::DIR_EXTERNAL_EXTENSIONS,
            data_dir().AppendASCII("external"));
#endif
  }

  void SetExternalExtensionsBlockedByPolicy(const bool block_external) {
    profile_->GetPrefs()->SetBoolean(pref_names::kBlockExternalExtensions,
                                     block_external);
  }

  void InitializeExtensionServiceWithUpdaterAndPrefs() {
    ExtensionServiceInitParams params;
    // Create prefs file to make the profile not new.
    params.prefs_content = "{}";
    params.autoupdate_enabled = true;
    InitializeExtensionService(std::move(params));
    service_->updater()->Start();
    content::RunAllTasksUntilIdle();
  }

  // ExtensionServiceTestBase overrides:
  void SetUp() override {
    ExtensionServiceTestBase::SetUp();
    test_server_ = std::make_unique<net::test_server::EmbeddedTestServer>();

    test_server_->RegisterRequestHandler(base::BindRepeating(
        &ExternalProviderImplTest::HandleRequest, base::Unretained(this)));
    ASSERT_TRUE(test_server_->Start());

    test_extension_cache_ = std::make_unique<ExtensionCacheFake>();

    extension_test_util::SetGalleryUpdateURL(
        test_server_->GetURL(kInAppPaymentsApp.update_path));
  }

  void AwaitCheckForExternalUpdates() {
    base::RunLoop run_loop;
    service_->set_external_updates_finished_callback_for_test(
        run_loop.QuitWhenIdleClosure());
    service_->CheckForExternalUpdates();
    run_loop.Run();
  }

 protected:
  std::unique_ptr<net::test_server::EmbeddedTestServer> test_server_;

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    GURL url = test_server_->GetURL(request.relative_url);
    for (const TestServerExtension& test_extension : kTestServerExtensions) {
      if (url.path() == test_extension.update_path) {
        auto response = std::make_unique<net::test_server::BasicHttpResponse>();
        response->set_code(net::HTTP_OK);
        response->set_content(CreateUpdateManifest(
            {UpdateManifestItem(test_extension.app_id)
                 .version(test_extension.version)
                 .codebase(
                     test_server_->GetURL(test_extension.app_path).spec())}));
        response->set_content_type("text/xml");
        return std::move(response);
      }
      if (url.path() == test_extension.app_path) {
        base::FilePath test_data_dir;
        base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
        std::string contents;
        base::ReadFileToString(
            test_data_dir.AppendASCII(test_extension.crx_path), &contents);
        auto response = std::make_unique<net::test_server::BasicHttpResponse>();
        response->set_code(net::HTTP_OK);
        response->set_content(contents);
        return std::move(response);
      }
    }

    return nullptr;
  }

  std::unique_ptr<base::ScopedPathOverride> external_externsions_overrides_;
  std::unique_ptr<ExtensionCacheFake> test_extension_cache_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // chromeos::ServicesCustomizationExternalLoader is hooked up as an
  // ExternalLoader and depends on a functioning StatisticsProvider.
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
#endif

#if BUILDFLAG(IS_WIN)
  // Registry key pointing to the external extension for Windows.
  base::win::RegKey external_extension_key_;
  registry_util::RegistryOverrideManager registry_override_manager_;
#endif
};

}  // namespace

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
TEST_F(ExternalProviderImplTest, InAppPayments) {
  InitServiceWithExternalProviders();

  AwaitCheckForExternalUpdates();

  EXPECT_TRUE(registry()->GetInstalledExtension(kInAppPaymentsApp.app_id));
  EXPECT_TRUE(service_->IsExtensionEnabled(kInAppPaymentsApp.app_id));
}

TEST_F(ExternalProviderImplTest, BlockedExternalUserProviders) {
  OverrideExternalExtensionsPath();
  InitServiceWithExternalProviders(true);

  AwaitCheckForExternalUpdates();

  EXPECT_FALSE(registry()->GetInstalledExtension(kExternalAppId));
}

TEST_F(ExternalProviderImplTest, NotBlockedExternalUserProviders) {
  OverrideExternalExtensionsPath();
  InitServiceWithExternalProviders(false);

  AwaitCheckForExternalUpdates();

  EXPECT_TRUE(registry()->GetInstalledExtension(kExternalAppId));
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

TEST_F(ExternalProviderImplTest, WebAppMigrationFlag) {
  InitService();

  const std::string json = base::StringPrintf(
      R"(
        {
          "%s": {
            "external_update_url": "%s",
            "web_app_migration_flag": "TestFeature"
          }
        }
      )",
      kGoodApp.app_id,
      test_server_->GetURL(kGoodApp.update_path).spec().c_str());
  service_->AddProviderForTesting(std::make_unique<ExternalProviderImpl>(
      service_,
      base::MakeRefCounted<ExternalTestingLoader>(
          json, base::FilePath(FILE_PATH_LITERAL("//absolute/path"))),
      profile_.get(), mojom::ManifestLocation::kExternalPref,
      mojom::ManifestLocation::kExternalPrefDownload, Extension::NO_FLAGS));

  // App is not installed, we should not install if the flag is enabled.
  {
    base::AutoReset<bool> testing_scope =
        web_app::SetPreinstalledAppInstallFeatureAlwaysEnabledForTesting();
    AwaitCheckForExternalUpdates();
    EXPECT_FALSE(registry()->GetInstalledExtension(kGoodApp.app_id));
  }

  // Disable the flag to install the app.
  {
    AwaitCheckForExternalUpdates();
    EXPECT_TRUE(registry()->GetInstalledExtension(kGoodApp.app_id));
  }

  // App is now installed, we should not uninstall if the flag is enabled.
  {
    base::AutoReset<bool> testing_scope =
        web_app::SetPreinstalledAppInstallFeatureAlwaysEnabledForTesting();
    AwaitCheckForExternalUpdates();
    EXPECT_TRUE(registry()->GetInstalledExtension(kGoodApp.app_id));
  }
}

}  // namespace extensions
