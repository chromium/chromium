// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/customization/customization_document.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ash/net/network_portal_detector_test_impl.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/pref_service_mock_factory.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/external_install_info.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::ExternalInstallInfoFile;
using extensions::ExternalInstallInfoUpdateUrl;
using extensions::mojom::ManifestLocation;
using ::testing::_;
using ::testing::Exactly;
using ::testing::Invoke;
using ::testing::Mock;

namespace {

const char kGoodStartupManifest[] =
    "{"
    "  \"version\": \"1.0\","
    "  \"initial_locale\" : \"en-US\","
    "  \"initial_timezone\" : \"US/Pacific\","
    "  \"keyboard_layout\" : \"xkb:us::eng\","
    "  \"setup_content\" : {"
    "    \"en-US\" : {"
    "      \"eula_page\" : \"file:///opt/oem/eula/en-US/eula.html\","
    "    },"
    "    \"ru-RU\" : {"
    "      \"eula_page\" : \"file:///opt/oem/eula/ru-RU/eula.html\","
    "    },"
    "    \"default\" : {"
    "      \"eula_page\" : \"file:///opt/oem/eula/en/eula.html\","
    "    },"
    "  },"
    "  \"hwid_map\" : ["
    "    {"
    "      \"hwid_mask\": \"ZGA*34\","
    "      \"initial_locale\" : \"ja\","
    "      \"initial_timezone\" : \"Asia/Tokyo\","
    "      \"keyboard_layout\" : \"mozc-jp\","
    "    },"
    "    {"
    "      \"hwid_mask\": \"Mario 1?3*\","
    "      \"initial_locale\" : \"ru-RU\","
    "      \"initial_timezone\" : \"Europe/Moscow\","
    "      \"keyboard_layout\" : \"xkb:ru::rus\","
    "    },"
    "  ],"
    "}";

const char kBadManifest[] = "{\"version\": \"1\"}";

const char kGoodServicesManifest[] =
    "{"
    "  \"version\": \"1.0\","
    "  \"default_apps\": [\n"
    "    \"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\",\n"
    "    {\n"
    "      \"id\": \"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\",\n"
    "      \"do_not_install_for_enterprise\": true\n"
    "    }\n"
    "  ],\n"
    "  \"localized_content\": {\n"
    "    \"en-US\": {\n"
    "      \"default_apps_folder_name\": \"EN-US OEM Name\"\n"
    "    },\n"
    "    \"en\": {\n"
    "      \"default_apps_folder_name\": \"EN OEM Name\"\n"
    "    },\n"
    "    \"default\": {\n"
    "      \"default_apps_folder_name\": \"Default OEM Name\"\n"
    "    }\n"
    "  }\n"
    "}";

const char kDummyCustomizationID[] = "test-dummy";

}  // anonymous namespace

namespace ash {

using ::testing::DoAll;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::_;

TEST(StartupCustomizationDocumentTest, Basic) {
  system::ScopedFakeStatisticsProvider fake_statistics_provider;

  // hardware_class selects the appropriate entry in hwid_map in the manifest.
  fake_statistics_provider.SetMachineStatistic("hardware_class", "Mario 12345");
  StartupCustomizationDocument customization(&fake_statistics_provider,
                                             kGoodStartupManifest);
  EXPECT_EQ("ru-RU", customization.initial_locale());
  EXPECT_EQ("Europe/Moscow", customization.initial_timezone());
  EXPECT_EQ("xkb:ru::rus", customization.keyboard_layout());

  EXPECT_EQ("file:///opt/oem/eula/en-US/eula.html",
            customization.GetEULAPage("en-US"));
  EXPECT_EQ("file:///opt/oem/eula/ru-RU/eula.html",
            customization.GetEULAPage("ru-RU"));
  EXPECT_EQ("file:///opt/oem/eula/en/eula.html",
            customization.GetEULAPage("ja"));
}

TEST(StartupCustomizationDocumentTest, VPD) {
  system::ScopedFakeStatisticsProvider fake_statistics_provider;

  // hardware_class selects the appropriate entry in hwid_map in the manifest.
  fake_statistics_provider.SetMachineStatistic("hardware_class", "Mario 12345");
  fake_statistics_provider.SetMachineStatistic("initial_locale", "ja");
  fake_statistics_provider.SetMachineStatistic("initial_timezone",
                                               "Asia/Tokyo");
  fake_statistics_provider.SetMachineStatistic("keyboard_layout", "mozc-jp");
  StartupCustomizationDocument customization(&fake_statistics_provider,
                                             kGoodStartupManifest);
  EXPECT_TRUE(customization.IsReady());
  EXPECT_EQ("ja", customization.initial_locale());
  EXPECT_EQ("Asia/Tokyo", customization.initial_timezone());
  EXPECT_EQ("mozc-jp", customization.keyboard_layout());
}

TEST(StartupCustomizationDocumentTest, BadManifest) {
  system::ScopedFakeStatisticsProvider fake_statistics_provider;
  StartupCustomizationDocument customization(&fake_statistics_provider,
                                             kBadManifest);
  EXPECT_FALSE(customization.IsReady());
}

class TestURLLoaderFactoryInterceptor {
 public:
  explicit TestURLLoaderFactoryInterceptor(
      network::TestURLLoaderFactory* factory) {
    factory->SetInterceptor(base::BindRepeating(
        &TestURLLoaderFactoryInterceptor::Intercept, base::Unretained(this)));
  }
  MOCK_METHOD1(Intercept, void(const network::ResourceRequest&));
};

class MockExternalProviderVisitor
    : public extensions::ExternalProviderInterface::VisitorInterface {
 public:
  MockExternalProviderVisitor() {}

  MOCK_METHOD1(OnExternalExtensionFileFound,
               bool(const ExternalInstallInfoFile&));
  MOCK_METHOD2(OnExternalExtensionUpdateUrlFound,
               bool(const ExternalInstallInfoUpdateUrl&, bool));
  MOCK_METHOD1(OnExternalProviderReady,
               void(const extensions::ExternalProviderInterface* provider));
  MOCK_METHOD4(OnExternalProviderUpdateComplete,
               void(const extensions::ExternalProviderInterface*,
                    const std::vector<ExternalInstallInfoUpdateUrl>&,
                    const std::vector<ExternalInstallInfoFile>&,
                    const std::set<std::string>& removed_extensions));
};

class ServicesCustomizationDocumentTest : public testing::Test {
 protected:
  ServicesCustomizationDocumentTest() = default;

  // testing::Test:
  void SetUp() override {
    ServicesCustomizationDocument::InitializeForTesting(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &loader_factory_));
    RunUntilIdle();

    const NetworkState* default_network =
        NetworkHandler::Get()->network_state_handler()->DefaultNetwork();
    std::string default_network_path =
        default_network ? default_network->path() : "";

    network_portal_detector::InitializeForTesting(&network_portal_detector_);
    std::string guid =
        default_network ? default_network->guid() : std::string();
    network_portal_detector_.SetDefaultNetworkForTesting(guid);

    TestingBrowserProcess::GetGlobal()->SetLocalState(&local_state_);
    RegisterLocalState(local_state_.registry());

    interceptor_ =
        std::make_unique<TestURLLoaderFactoryInterceptor>(&loader_factory_);
  }

  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
    network_portal_detector::InitializeForTesting(nullptr);
    loader_factory_.ClearResponses();
    interceptor_.reset();

    ServicesCustomizationDocument::ShutdownForTesting();
  }

  void RunUntilIdle() {
    base::RunLoop().RunUntilIdle();
  }

  void AddCustomizationIdToVp(const std::string& id) {
    fake_statistics_provider_.SetMachineStatistic(system::kCustomizationIdKey,
                                                  id);
  }

  void AddExpectedManifest(const std::string& id,
                           const std::string& manifest) {
    GURL url(base::StringPrintf(ServicesCustomizationDocument::kManifestUrl,
                                id.c_str()));

    auto response_head = network::mojom::URLResponseHead::New();
    response_head->headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
    response_head->headers->SetHeader("Content-Type", "application/json");
    loader_factory_.AddResponse(url, std::move(response_head), manifest,
                                network::URLLoaderCompletionStatus(net::OK));
    EXPECT_CALL(*interceptor_, Intercept).Times(Exactly(1));
  }

  void AddManifestNotFound(const std::string& id) {
    GURL url(base::StringPrintf(ServicesCustomizationDocument::kManifestUrl,
                                id.c_str()));

    auto response_head = network::mojom::URLResponseHead::New();
    response_head->headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
    response_head->headers->SetHeader("Content-Type", "application/json");
    response_head->headers->ReplaceStatusLine("HTTP/1.1 404 Not found");
    loader_factory_.AddResponse(url, std::move(response_head), std::string(),
                                network::URLLoaderCompletionStatus(net::OK));
    EXPECT_CALL(*interceptor_, Intercept).Times(Exactly(1));
  }

  std::unique_ptr<TestingProfile> CreateProfile() {
    TestingProfile::Builder profile_builder;
    sync_preferences::PrefServiceMockFactory factory;
    auto registry = base::MakeRefCounted<user_prefs::PrefRegistrySyncable>();
    std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs(
        factory.CreateSyncable(registry.get()));
    RegisterUserProfilePrefs(registry.get());
    profile_builder.SetPrefService(std::move(prefs));
    std::unique_ptr<TestingProfile> profile = profile_builder.Build();
    // Make sure we have a Profile Manager.
    TestingBrowserProcess::GetGlobal()->SetProfileManager(
        std::make_unique<ProfileManagerWithoutInit>(profile->GetPath()));
    return profile;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  NetworkHandlerTestHelper network_handler_test_helper_;
  system::ScopedFakeStatisticsProvider fake_statistics_provider_;
  ScopedCrosSettingsTestHelper scoped_cros_settings_test_helper_;
  TestingPrefServiceSimple local_state_;
  network::TestURLLoaderFactory loader_factory_;
  std::unique_ptr<TestURLLoaderFactoryInterceptor> interceptor_;
  NetworkPortalDetectorTestImpl network_portal_detector_;
};

TEST_F(ServicesCustomizationDocumentTest, Basic) {
  AddCustomizationIdToVp(kDummyCustomizationID);
  AddExpectedManifest(kDummyCustomizationID, kGoodServicesManifest);

  ServicesCustomizationDocument* doc =
      ServicesCustomizationDocument::GetInstance();
  EXPECT_FALSE(doc->IsReady());

  doc->StartFetching();
  RunUntilIdle();
  EXPECT_TRUE(doc->IsReady());

  GURL wallpaper_url;
  EXPECT_FALSE(doc->GetDefaultWallpaperUrl(&wallpaper_url));
  EXPECT_EQ("", wallpaper_url.spec());

  auto default_apps = doc->GetDefaultApps();
  ASSERT_TRUE(default_apps);
  EXPECT_EQ(default_apps->size(), 2u);

  const base::Value::Dict* app_entry =
      default_apps->FindDict("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  ASSERT_TRUE(app_entry);
  EXPECT_EQ(app_entry->size(), 1u);
  EXPECT_TRUE(
      app_entry->Find(extensions::ExternalProviderImpl::kExternalUpdateUrl) !=
      nullptr);

  app_entry = default_apps->FindDict("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
  ASSERT_TRUE(app_entry);
  EXPECT_EQ(app_entry->size(), 2u);
  EXPECT_TRUE(
      app_entry->Find(extensions::ExternalProviderImpl::kExternalUpdateUrl) !=
      nullptr);
  EXPECT_TRUE(
      app_entry->Find(
          extensions::ExternalProviderImpl::kDoNotInstallForEnterprise) !=
      nullptr);

  EXPECT_EQ("EN-US OEM Name", doc->GetOemAppsFolderName("en-US"));
  EXPECT_EQ("EN OEM Name", doc->GetOemAppsFolderName("en"));
  EXPECT_EQ("Default OEM Name", doc->GetOemAppsFolderName("ru"));
}

TEST_F(ServicesCustomizationDocumentTest, NoCustomizationIdInVpd) {
  ServicesCustomizationDocument* doc =
      ServicesCustomizationDocument::GetInstance();
  EXPECT_FALSE(doc->IsReady());

  std::unique_ptr<TestingProfile> profile = CreateProfile();
  extensions::ExternalLoader* loader = doc->CreateExternalLoader(profile.get());
  EXPECT_TRUE(loader);

  MockExternalProviderVisitor visitor;
  auto provider = std::make_unique<extensions::ExternalProviderImpl>(
      &visitor, loader, profile.get(), ManifestLocation::kExternalPref,
      ManifestLocation::kExternalPrefDownload,
      extensions::Extension::FROM_WEBSTORE |
          extensions::Extension::WAS_INSTALLED_BY_DEFAULT);

  EXPECT_CALL(visitor, OnExternalExtensionFileFound(_)).Times(0);
  EXPECT_CALL(visitor, OnExternalExtensionUpdateUrlFound(_, _)).Times(0);
  EXPECT_CALL(visitor, OnExternalProviderReady(_))
      .Times(1);
  EXPECT_CALL(visitor, OnExternalProviderUpdateComplete(_, _, _, _)).Times(0);

  // Manually request a load.
  RunUntilIdle();
  loader->StartLoading();
  Mock::VerifyAndClearExpectations(&visitor);

  RunUntilIdle();
  // Empty customization is used when there is no customization ID in VPD.
  EXPECT_TRUE(doc->IsReady());
}

TEST_F(ServicesCustomizationDocumentTest, DefaultApps) {
  AddCustomizationIdToVp(kDummyCustomizationID);
  AddExpectedManifest(kDummyCustomizationID, kGoodServicesManifest);

  ServicesCustomizationDocument* doc =
      ServicesCustomizationDocument::GetInstance();
  EXPECT_FALSE(doc->IsReady());

  std::unique_ptr<TestingProfile> profile = CreateProfile();
  extensions::ExternalLoader* loader = doc->CreateExternalLoader(profile.get());
  EXPECT_TRUE(loader);

  app_list::AppListSyncableServiceFactory::GetInstance()
      ->SetTestingFactoryAndUse(
          profile.get(),
          base::BindRepeating(
              &app_list::AppListSyncableServiceFactory::BuildInstanceFor));

  MockExternalProviderVisitor visitor;
  auto provider = std::make_unique<extensions::ExternalProviderImpl>(
      &visitor, loader, profile.get(), ManifestLocation::kExternalPref,
      ManifestLocation::kExternalPrefDownload,
      extensions::Extension::FROM_WEBSTORE |
          extensions::Extension::WAS_INSTALLED_BY_DEFAULT);

  EXPECT_CALL(visitor, OnExternalExtensionFileFound(_)).Times(0);
  EXPECT_CALL(visitor, OnExternalExtensionUpdateUrlFound(_, _)).Times(0);
  EXPECT_CALL(visitor, OnExternalProviderReady(_))
      .Times(1);
  EXPECT_CALL(visitor, OnExternalProviderUpdateComplete(_, _, _, _)).Times(0);

  // Manually request a load.
  loader->StartLoading();
  Mock::VerifyAndClearExpectations(&visitor);

  EXPECT_CALL(visitor, OnExternalExtensionFileFound(_)).Times(0);
  EXPECT_CALL(visitor, OnExternalExtensionUpdateUrlFound(_, _)).Times(2);
  EXPECT_CALL(visitor, OnExternalProviderReady(_))
      .Times(1);
  EXPECT_CALL(visitor, OnExternalProviderUpdateComplete(_, _, _, _)).Times(0);

  RunUntilIdle();
  EXPECT_TRUE(doc->IsReady());

  app_list::AppListSyncableService* service =
      app_list::AppListSyncableServiceFactory::GetForProfile(profile.get());
  ASSERT_TRUE(service);
  EXPECT_EQ("EN OEM Name", service->GetOemFolderNameForTest());
}

TEST_F(ServicesCustomizationDocumentTest, CustomizationManifestNotFound) {
  AddCustomizationIdToVp(kDummyCustomizationID);
  AddManifestNotFound(kDummyCustomizationID);

  ServicesCustomizationDocument* doc =
      ServicesCustomizationDocument::GetInstance();
  EXPECT_FALSE(doc->IsReady());

  std::unique_ptr<TestingProfile> profile = CreateProfile();
  extensions::ExternalLoader* loader = doc->CreateExternalLoader(profile.get());
  EXPECT_TRUE(loader);

  MockExternalProviderVisitor visitor;
  auto provider = std::make_unique<extensions::ExternalProviderImpl>(
      &visitor, loader, profile.get(), ManifestLocation::kExternalPref,
      ManifestLocation::kExternalPrefDownload,
      extensions::Extension::FROM_WEBSTORE |
          extensions::Extension::WAS_INSTALLED_BY_DEFAULT);

  EXPECT_CALL(visitor, OnExternalExtensionFileFound(_)).Times(0);
  EXPECT_CALL(visitor, OnExternalExtensionUpdateUrlFound(_, _)).Times(0);
  EXPECT_CALL(visitor, OnExternalProviderReady(_))
      .Times(1);
  EXPECT_CALL(visitor, OnExternalProviderUpdateComplete(_, _, _, _)).Times(0);

  // Manually request a load.
  loader->StartLoading();
  Mock::VerifyAndClearExpectations(&visitor);

  EXPECT_CALL(visitor, OnExternalExtensionFileFound(_)).Times(0);
  EXPECT_CALL(visitor, OnExternalExtensionUpdateUrlFound(_, _)).Times(0);
  EXPECT_CALL(visitor, OnExternalProviderReady(_))
      .Times(1);
  EXPECT_CALL(visitor, OnExternalProviderUpdateComplete(_, _, _, _)).Times(0);

  RunUntilIdle();
  EXPECT_TRUE(doc->IsReady());
}

}  // namespace ash
