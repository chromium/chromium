// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_provider_impl.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_path_override.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_test_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/pref_names.h"
#include "extensions/browser/updater/extension_cache_fake.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/customization/customization_document.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "chromeos/system/statistics_provider.h"
#include "components/user_manager/scoped_user_manager.h"
#endif

#if defined(OS_WIN)
#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#endif

namespace extensions {

namespace {

const char kManifestPath[] = "/update_manifest";
const char kAppPath[] = "/app.crx";

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
const char kExternalAppId[] = "kekdneafjmhmndejhmbcadfiiofngffo";
#endif

#if defined(OS_WIN)
const char kExternalAppCrxPath[] =
    "external\\kekdneafjmhmndejhmbcadfiiofngffo.crx";
const wchar_t kExternalAppRegistryKey[] =
    L"Software\\Google\\Chrome\\Extensions\\kekdneafjmhmndejhmbcadfiiofngffo";
#endif

class ExternalProviderImplTest : public ExtensionServiceTestBase {
 public:
  ExternalProviderImplTest() {}
  ~ExternalProviderImplTest() override {}

  void InitServiceWithExternalProviders(
      const base::Optional<bool> block_external = base::nullopt) {
#if defined(OS_CHROMEOS)
    user_manager::ScopedUserManager scoped_user_manager(
        std::make_unique<chromeos::FakeChromeUserManager>());
#endif
    InitializeExtensionServiceWithUpdaterAndPrefs();

    service()->updater()->SetExtensionCacheForTesting(
        test_extension_cache_.get());

    // Don't install default apps. Some of the default apps are downloaded from
    // the webstore, ignoring the url we pass to kAppsGalleryUpdateURL, which
    // would cause the external updates to never finish install.
    profile_->GetPrefs()->SetString(prefs::kDefaultApps, "");

    if (block_external.has_value())
      SetExternalExtensionsBlockedByPolicy(block_external.value());

    ProviderCollection providers;
    extensions::ExternalProviderImpl::CreateExternalProviders(
        service_, profile_.get(), &providers);

    for (std::unique_ptr<ExternalProviderInterface>& provider : providers)
      service_->AddProviderForTesting(std::move(provider));
  }

  void OverrideExternalExtensionsPath() {
    // Windows doesn't use the provider that installs the |kExternalAppId|
    // extension implicitly, so to test that the blocking policy works on
    // Windows it is installed through a Windows-specific registry provider.
#if defined(OS_WIN)
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
    external_externsions_overrides_.reset(new base::ScopedPathOverride(
        chrome::DIR_EXTERNAL_EXTENSIONS, data_dir().AppendASCII("external")));
#endif
  }

  void SetExternalExtensionsBlockedByPolicy(const bool block_external) {
    profile_->GetPrefs()->SetBoolean(pref_names::kBlockExternalExtensions,
                                     block_external);
  }

  void InitializeExtensionServiceWithUpdaterAndPrefs() {
    ExtensionServiceInitParams params = CreateDefaultInitParams();
    params.autoupdate_enabled = true;
    // Create prefs file to make the profile not new.
    const char prefs[] = "{}";
    EXPECT_EQ(base::WriteFile(params.pref_file, prefs, sizeof(prefs)),
              int(sizeof(prefs)));
    InitializeExtensionService(params);
    service_->updater()->Start();
    content::RunAllTasksUntilIdle();
  }

  // ExtensionServiceTestBase overrides:
  void SetUp() override {
    ExtensionServiceTestBase::SetUp();
    test_server_ = std::make_unique<net::test_server::EmbeddedTestServer>();

    test_server_->RegisterRequestHandler(
        base::Bind(&ExternalProviderImplTest::HandleRequest,
                   base::Unretained(this)));
    ASSERT_TRUE(test_server_->Start());

    test_extension_cache_.reset(new ExtensionCacheFake());

    extension_test_util::SetGalleryUpdateURL(
        test_server_->GetURL(kManifestPath));
  }

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    GURL url = test_server_->GetURL(request.relative_url);
    if (url.path() == kManifestPath) {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      response->set_content(base::StringPrintf(
          "<?xml version='1.0' encoding='UTF-8'?>\n"
          "<gupdate xmlns='http://www.google.com/update2/response' "
              "protocol='2.0'>\n"
          "  <app appid='%s'>\n"
          "    <updatecheck codebase='%s' version='1.0' />\n"
          "  </app>\n"
          "</gupdate>",
          extension_misc::kInAppPaymentsSupportAppId,
          test_server_->GetURL(kAppPath).spec().c_str()));
      response->set_content_type("text/xml");
      return std::move(response);
    }
    if (url.path() == kAppPath) {
      base::FilePath test_data_dir;
      base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
      std::string contents;
      base::ReadFileToString(
          test_data_dir.AppendASCII("extensions/dummyiap.crx"),
          &contents);
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      response->set_content(contents);
      return std::move(response);
    }

    return nullptr;
  }

  std::unique_ptr<base::ScopedPathOverride> external_externsions_overrides_;
  std::unique_ptr<net::test_server::EmbeddedTestServer> test_server_;
  std::unique_ptr<ExtensionCacheFake> test_extension_cache_;

#if defined(OS_CHROMEOS)
  // chromeos::ServicesCustomizationExternalLoader is hooked up as an
  // extensions::ExternalLoader and depends on a functioning StatisticsProvider.
  chromeos::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
#endif

#if defined(OS_WIN)
  // Registry key pointing to the external extension for Windows.
  base::win::RegKey external_extension_key_;
  registry_util::RegistryOverrideManager registry_override_manager_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ExternalProviderImplTest);
};

}  // namespace

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
TEST_F(ExternalProviderImplTest, InAppPayments) {
  InitServiceWithExternalProviders();

  base::RunLoop run_loop;
  service_->set_external_updates_finished_callback_for_test(
      run_loop.QuitWhenIdleClosure());
  service_->CheckForExternalUpdates();
  run_loop.Run();

  EXPECT_TRUE(registry()->GetInstalledExtension(
      extension_misc::kInAppPaymentsSupportAppId));
  EXPECT_TRUE(service_->IsExtensionEnabled(
      extension_misc::kInAppPaymentsSupportAppId));
}

TEST_F(ExternalProviderImplTest, BlockedExternalUserProviders) {
  OverrideExternalExtensionsPath();
  InitServiceWithExternalProviders(true);

  base::RunLoop run_loop;
  service_->set_external_updates_finished_callback_for_test(
      run_loop.QuitWhenIdleClosure());
  service_->CheckForExternalUpdates();
  run_loop.Run();

  EXPECT_FALSE(registry()->GetInstalledExtension(kExternalAppId));
}

TEST_F(ExternalProviderImplTest, NotBlockedExternalUserProviders) {
  OverrideExternalExtensionsPath();
  InitServiceWithExternalProviders(false);

  base::RunLoop run_loop;
  service_->set_external_updates_finished_callback_for_test(
      run_loop.QuitWhenIdleClosure());
  service_->CheckForExternalUpdates();
  run_loop.Run();

  EXPECT_TRUE(registry()->GetInstalledExtension(kExternalAppId));
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

}  // namespace extensions
