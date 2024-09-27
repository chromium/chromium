// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/policy/arc_policy_bridge.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_session_runner.h"
#include "ash/components/arc/test/arc_util_test_support.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_arc_session.h"
#include "ash/components/arc/test/fake_policy_instance.h"
#include "ash/constants/ash_switches.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_command_line.h"
#include "base/values.h"
#include "chrome/browser/ash/arc/enterprise/cert_store/cert_store_service.h"
#include "chrome/browser/ash/arc/enterprise/cert_store/cert_store_service_factory.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/test/test_arc_session_manager.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/policy/developer_tools_policy_handler.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/upstart/fake_upstart_client.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/remote_commands/remote_commands_queue.h"
#include "components/policy/policy_constants.h"
#include "components/session_manager/core/session_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Mock;
using testing::ReturnRef;

namespace arc {

namespace {

constexpr char kFakeONC[] =
    "{\"NetworkConfigurations\":["
    "{\"GUID\":\"{485d6076-dd44-6b6d-69787465725f5040}\","
    "\"Type\":\"WiFi\","
    "\"Name\":\"My WiFi Network\","
    "\"WiFi\":{"
    "\"HexSSID\":\"737369642D6E6F6E65\","  // "ssid-none"
    "\"Security\":\"None\"}"
    "}"
    "],"
    "\"GlobalNetworkConfiguration\":{"
    "\"AllowOnlyPolicyNetworksToAutoconnect\":true,"
    "},"
    "\"Certificates\":["
    "{ \"GUID\":\"{f998f760-272b-6939-4c2beffe428697ac}\","
    "\"PKCS12\":\"abc\","
    "\"Type\":\"Client\"},"
    "{\"Type\":\"Authority\","
    "\"TrustBits\":[\"Web\"],"
    "\"X509\":\"TWFuIGlzIGRpc3Rpbmd1aXNoZWQsIG5vdCBvbmx5IGJ5IGhpcyByZWFzb24sIGJ"
    "1dCBieSB0aGlzIHNpbmd1bGFyIHBhc3Npb24gZnJvbSBvdGhlciBhbmltYWxzLCB3aGljaCBpc"
    "yBhIGx1c3Qgb2YgdGhlIG1pbmQsIHRoYXQgYnkgYSBwZXJzZXZlcmFuY2Ugb2YgZGVsaWdodCB"
    "pbiB0aGUgY29udGludWVkIGFuZCBpbmRlZmF0aWdhYmxlIGdlbmVyYXRpb24gb2Yga25vd2xlZ"
    "GdlLCBleGNlZWRzIHRoZSBzaG9ydCB2ZWhlbWVuY2Ugb2YgYW55IGNhcm5hbCBwbGVhc3VyZS4"
    "=\","
    "\"GUID\":\"{00f79111-51e0-e6e0-76b3b55450d80a1b}\"}"
    "]}";

constexpr char kPolicyCompliantResponse[] = "{ \"policyCompliant\": true }";

constexpr char kFakeCertName[] = "cert_name";
constexpr char kRequiredKeyPairsEmpty[] = "\"requiredKeyPairs\":[]";
constexpr char kRequiredKeyPairsFormat[] =
    "\"requiredKeyPairs\":[{\"alias\":\"%s\"}]";

constexpr char kChoosePrivateKeyRulesFormat[] =
    "\"choosePrivateKeyRules\":["
    "{\"packageNames\":[\"%s\"],"
    "\"privateKeyAlias\":\"%s\"}]";

constexpr char kMountPhysicalMediaDisabledPolicySetting[] =
    "\"mountPhysicalMediaDisabled\":false";

constexpr char kSupervisedUserPlayStoreModePolicySetting[] =
    "\"playStoreMode\":\"SUPERVISED\"";

constexpr char kTestUserEmail[] = "user@gmail.com";

constexpr char kChromeAppId[] = "chromeappid";
constexpr char kAndroidAppId[] = "android.app.id";

void AddKeyPermissionForAppId(base::Value::Dict& key_permissions,
                              const std::string& app_id,
                              bool allowed) {
  base::Value::Dict cert_key_permission;
  cert_key_permission.Set("allowCorporateKeyUsage", base::Value(allowed));
  key_permissions.Set(app_id, std::move(cert_key_permission));
}

MATCHER_P(ValueEquals, expected, "value matches") {
  return *expected == *arg;
}

class MockArcPolicyBridgeObserver : public ArcPolicyBridge::Observer {
 public:
  MockArcPolicyBridgeObserver() = default;

  MockArcPolicyBridgeObserver(const MockArcPolicyBridgeObserver&) = delete;
  MockArcPolicyBridgeObserver& operator=(const MockArcPolicyBridgeObserver&) =
      delete;

  ~MockArcPolicyBridgeObserver() override = default;

  MOCK_METHOD1(OnPolicySent, void(const std::string&));
  MOCK_METHOD1(OnComplianceReportReceived, void(const base::Value*));
  MOCK_METHOD1(OnReportDPCVersion, void(const std::string&));
};

// Helper class to define callbacks that verify that they were run.
// Wraps a bool initially set to |false| and verifies that it's been set to
// |true| before destruction.
class CheckedBoolean {
 public:
  CheckedBoolean() {}

  CheckedBoolean(const CheckedBoolean&) = delete;
  CheckedBoolean& operator=(const CheckedBoolean&) = delete;

  ~CheckedBoolean() { EXPECT_TRUE(value_); }

  void set_value(bool value) { value_ = value; }

 private:
  bool value_ = false;
};

void ExpectString(std::unique_ptr<CheckedBoolean> was_run,
                  const std::string& expected,
                  const std::string& received) {
  EXPECT_EQ(expected, received);
  was_run->set_value(true);
}

void ExpectStringWithClosure(base::OnceClosure quit_closure,
                             std::unique_ptr<CheckedBoolean> was_run,
                             const std::string& expected,
                             const std::string& received) {
  EXPECT_EQ(expected, received);
  was_run->set_value(true);
  std::move(quit_closure).Run();
}

arc::ArcPolicyBridge::GetPoliciesCallback PolicyStringCallback(
    const std::string& expected) {
  auto was_run = std::make_unique<CheckedBoolean>();
  return base::BindOnce(&ExpectString, std::move(was_run), expected);
}

arc::ArcPolicyBridge::ReportComplianceCallback PolicyComplianceCallback(
    base::OnceClosure quit_closure) {
  auto was_run = std::make_unique<CheckedBoolean>();
  return base::BindOnce(&ExpectStringWithClosure, std::move(quit_closure),
                        std::move(was_run), kPolicyCompliantResponse);
}

}  // namespace

class ArcPolicyBridgeTestBase {
 public:
  ArcPolicyBridgeTestBase() = default;

  ArcPolicyBridgeTestBase(const ArcPolicyBridgeTestBase&) = delete;
  ArcPolicyBridgeTestBase& operator=(const ArcPolicyBridgeTestBase&) = delete;

  void DoSetUp(bool is_affiliated) {
    SetArcAvailableCommandLineForTesting(
        base::CommandLine::ForCurrentProcess());

    // Set up fake StatisticsProvider.
    ash::system::StatisticsProvider::SetTestProvider(&statistics_provider_);

    // Set up ArcBridgeService.
    bridge_service_ = std::make_unique<ArcBridgeService>();
    EXPECT_CALL(policy_service_,
                GetPolicies(policy::PolicyNamespace(
                    policy::POLICY_DOMAIN_CHROME, std::string())))
        .WillRepeatedly(ReturnRef(policy_map_));
    EXPECT_CALL(policy_service_, AddObserver(policy::POLICY_DOMAIN_CHROME, _))
        .Times(1);
    EXPECT_CALL(policy_service_,
                RemoveObserver(policy::POLICY_DOMAIN_CHROME, _))
        .Times(1);

    // Set up user profile for ReportCompliance() tests.
    fake_user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());
    const AccountId account_id(
        AccountId::FromUserEmailGaiaId(kTestUserEmail, "1111111111"));
    fake_user_manager_->AddUserWithAffiliation(account_id, is_affiliated);
    fake_user_manager_->LoginUser(account_id);
    testing_profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(testing_profile_manager_->SetUp());
    profile_ = testing_profile_manager_->CreateTestingProfile(
        kTestUserEmail, IdentityTestEnvironmentProfileAdaptor::
                            GetIdentityTestEnvironmentFactories());
    ASSERT_TRUE(profile_);

    auto identity_test_env_profile_adaptor =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_);
    identity_test_env_profile_adaptor->identity_test_env()
        ->MakePrimaryAccountAvailable(kTestUserEmail,
                                      signin::ConsentLevel::kSignin);

    cert_store_service_ = GetCertStoreService();

    // Init ArcSessionManager for testing.
    ash::ConciergeClient::InitializeFake(/*fake_cicerone_client=*/nullptr);
    arc_session_manager_ =
        CreateTestArcSessionManager(std::make_unique<ArcSessionRunner>(
            base::BindRepeating(FakeArcSession::Create)));
    arc_session_manager()->SetProfile(profile());
    arc_session_manager()->Initialize();

    // TODO(hidehiko): Use Singleton instance tied to BrowserContext.
    policy_bridge_ = std::make_unique<ArcPolicyBridge>(
        profile_, bridge_service_.get(), &policy_service_);
    policy_bridge_->OverrideIsManagedForTesting(true);
    policy_bridge_->AddObserver(&observer_);
    instance_guid_ = policy_bridge_->GetInstanceGuidForTesting();

    policy_instance_ = std::make_unique<FakePolicyInstance>();
    bridge_service_->policy()->SetInstance(policy_instance_.get());
    WaitForInstanceReady(bridge_service_->policy());
  }

  void DoTearDown() {
    bridge_service_->policy()->CloseInstance(policy_instance_.get());
    policy_instance_.reset();
    policy_bridge_->RemoveObserver(&observer_);
    policy_bridge_.reset();
    arc_session_manager()->Shutdown();
    arc_session_manager_.reset();
    ash::ConciergeClient::Shutdown();
    testing_profile_manager_.reset();
  }

 protected:
  void GetPoliciesAndVerifyResult(const std::string& expected_policy_json) {
    Mock::VerifyAndClearExpectations(&observer_);
    EXPECT_CALL(observer_, OnPolicySent(expected_policy_json));
    policy_bridge()->GetPolicies(PolicyStringCallback(expected_policy_json));
    EXPECT_EQ(expected_policy_json,
              policy_bridge()->get_arc_policy_for_reporting());
    Mock::VerifyAndClearExpectations(&observer_);
  }

  void ReportDPCVersionAndVerifyObserverCallback(const std::string& version) {
    Mock::VerifyAndClearExpectations(&observer_);
    EXPECT_CALL(observer_, OnReportDPCVersion(version));

    policy_bridge()->ReportDPCVersion(version);

    EXPECT_EQ(version, policy_bridge()->get_arc_dpc_version());
    Mock::VerifyAndClearExpectations(&observer_);
  }

  void ReportComplianceAndVerifyObserverCallback(
      const std::string& compliance_report) {
    Mock::VerifyAndClearExpectations(&observer_);
    std::optional<base::Value> compliance_report_value =
        base::JSONReader::Read(compliance_report);
    if (compliance_report_value && compliance_report_value->is_dict()) {
      EXPECT_CALL(observer_, OnComplianceReportReceived(
                                 ValueEquals(&*compliance_report_value)));
    } else {
      EXPECT_CALL(observer_, OnComplianceReportReceived(_)).Times(0);
    }
    policy_bridge()->ReportCompliance(
        compliance_report, PolicyComplianceCallback(run_loop().QuitClosure()));
    run_loop().Run();
    Mock::VerifyAndClearExpectations(&observer_);

    if (compliance_report_value) {
      std::optional<base::Value> saved_compliance_report_value =
          base::JSONReader::Read(
              policy_bridge()->get_arc_policy_compliance_report());
      ASSERT_TRUE(saved_compliance_report_value);
      EXPECT_EQ(*compliance_report_value, *saved_compliance_report_value);
    } else {
      EXPECT_TRUE(policy_bridge()->get_arc_policy_compliance_report().empty());
    }
  }

  // Specifies a testing factory for CertStoreService and returns instance.
  // Returns nullptr by default.
  // Override if the test wants to use a real cert store service.
  virtual CertStoreService* GetCertStoreService() {
    return static_cast<CertStoreService*>(
        CertStoreServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(),
            base::BindRepeating(
                [](content::BrowserContext* profile)
                    -> std::unique_ptr<KeyedService> { return nullptr; })));
  }

  ArcPolicyBridge* policy_bridge() { return policy_bridge_.get(); }
  const std::string& instance_guid() { return instance_guid_; }
  FakePolicyInstance* policy_instance() { return policy_instance_.get(); }
  policy::PolicyMap& policy_map() { return policy_map_; }
  base::RunLoop& run_loop() { return run_loop_; }
  TestingProfile* profile() { return profile_; }
  ArcBridgeService* bridge_service() { return bridge_service_.get(); }
  CertStoreService* cert_store_service() { return cert_store_service_; }
  ash::system::FakeStatisticsProvider statistics_provider_;
  ArcSessionManager* arc_session_manager() {
    return arc_session_manager_.get();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
  session_manager::SessionManager session_manager_;
  std::unique_ptr<TestingProfileManager> testing_profile_manager_;
  base::RunLoop run_loop_;
  raw_ptr<TestingProfile, DanglingUntriaged> profile_;
  std::unique_ptr<ArcBridgeService> bridge_service_;
  raw_ptr<CertStoreService, DanglingUntriaged>
      cert_store_service_;  // Not owned.

  std::unique_ptr<ArcSessionManager> arc_session_manager_;
  std::unique_ptr<ArcPolicyBridge> policy_bridge_;
  std::string instance_guid_;
  MockArcPolicyBridgeObserver observer_;
  // Always keep policy_instance_ below bridge_service_, so that
  // policy_instance_ is destructed first. It needs to remove itself as
  // observer.
  std::unique_ptr<FakePolicyInstance> policy_instance_;
  policy::PolicyMap policy_map_;
  policy::MockPolicyService policy_service_;
};

class ArcPolicyBridgeTest : public ArcPolicyBridgeTestBase,
                            public testing::Test {
 public:
  void SetUp() override { DoSetUp(true /* affiliated */); }

  void TearDown() override { DoTearDown(); }
};

class ArcPolicyBridgeAffiliatedTest : public ArcPolicyBridgeTestBase,
                                      public testing::TestWithParam<bool> {
 public:
  ArcPolicyBridgeAffiliatedTest() : is_affiliated_(GetParam()) {}
  void SetUp() override { DoSetUp(is_affiliated_); }

  void TearDown() override { DoTearDown(); }

 protected:
  void GetPoliciesAndVerifyResultWithAffiliation(
      const std::string& expected_policy_json_affiliated,
      const std::string& expected_policy_json_not_affiliated) {
    if (is_affiliated_)
      GetPoliciesAndVerifyResult(expected_policy_json_affiliated);
    else
      GetPoliciesAndVerifyResult(expected_policy_json_not_affiliated);
  }
  const bool is_affiliated_;
};

// Tests required key pair policy.
class ArcPolicyBridgeCertStoreTest : public ArcPolicyBridgeTest {
 protected:
  CertStoreService* GetCertStoreService() override {
    return static_cast<CertStoreService*>(
        CertStoreServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(),
            base::BindRepeating([](content::BrowserContext* profile)
                                    -> std::unique_ptr<KeyedService> {
              return std::make_unique<CertStoreService>(nullptr);
            })));
  }
};

TEST_F(ArcPolicyBridgeTest, UnmanagedTest) {
  policy_bridge()->OverrideIsManagedForTesting(false);
  GetPoliciesAndVerifyResult("");
}

TEST_F(ArcPolicyBridgeTest, EmptyPolicyTest) {
  // No policy is set, result should be empty except for the instance GUID.
  GetPoliciesAndVerifyResult("{\"apkCacheEnabled\":true,\"guid\":\"" +
                             instance_guid() + "\"," +
                             kMountPhysicalMediaDisabledPolicySetting + "}");
}

TEST_F(ArcPolicyBridgeTest, ArcPolicyTest) {
  policy_map().Set(
      policy::key::kArcPolicy, policy::POLICY_LEVEL_MANDATORY,
      policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
      base::Value("{\"applications\":"
                  "[{\"packageName\":\"com.google.android.apps.youtube.kids\","
                  "\"installType\":\"REQUIRED\","
                  "\"lockTaskAllowed\":false,"
                  "\"permissionGrants\":[]"
                  "}],"
                  "\"defaultPermissionPolicy\":\"GRANT\""
                  "}"),
      nullptr);
  GetPoliciesAndVerifyResult(
      "{\"apkCacheEnabled\":true,"
      "\"applications\":"
      "[{\"installType\":\"REQUIRED\","
      "\"lockTaskAllowed\":false,"
      "\"packageName\":\"com.google.android.apps.youtube.kids\","
      "\"permissionGrants\":[]"
      "}],"
      "\"defaultPermissionPolicy\":\"GRANT\","
      "\"guid\":\"" +
      instance_guid() + "\"," + kMountPhysicalMediaDisabledPolicySetting + "}");
}

TEST_F(ArcPolicyBridgeTest, HompageLocationTest) {
  // This policy will not be passed on, result should be empty except for the
  // instance GUID.
  policy_map().Set(policy::key::kHomepageLocation,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD,
                   base::Value("http://chromium.org"), nullptr);
  GetPoliciesAndVerifyResult("{\"apkCacheEnabled\":true,\"guid\":\"" +
                             instance_guid() + "\"," +
                             kMountPhysicalMediaDisabledPolicySetting + "}");
}

TEST_F(ArcPolicyBridgeTest, DisableScreenshotsTest) {
  policy_map().Set(policy::key::kDisableScreenshots,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD, base::Value(true), nullptr);
  GetPoliciesAndVerifyResult("{\"apkCacheEnabled\":true,\"guid\":\"" +
                             instance_guid() + "\"," +
                             kMountPhysicalMediaDisabledPolicySetting + "," +
                             "\"screenCaptureDisabled\":true}");
}

TEST_F(ArcPolicyBridgeTest, DisablePrintingTest) {
  policy_map().Set(policy::key::kPrintingEnabled,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD, base::Value(false), nullptr);
  GetPoliciesAndVerifyResult("{\"apkCacheEnabled\":true,\"guid\":\"" +
                             instance_guid() + "\"," +
                             kMountPhysicalMediaDisabledPolicySetting + "," +
                             "\"printingDisabled\":true}");
}

TEST_F(ArcPolicyBridgeTest, VideoCaptureAllowedTest) {
  policy_map().Set(policy::key::kVideoCaptureAllowed,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD, base::Value(false), nullptr);
  GetPoliciesAndVerifyResult(
      "{\"apkCacheEnabled\":true,\"cameraDisabled\":true,\"guid\":\"" +
      instance_guid() + "\"," + kMountPhysicalMediaDisabledPolicySetting + "}");
}

TEST_F(ArcPolicyBridgeTest, AudioCaptureAllowedTest) {
  policy_map().Set(policy::key::kAudioCaptureAllowed,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD, base::Value(false), nullptr);
  GetPoliciesAndVerifyResult("{\"apkCacheEnabled\":true,\"guid\":\"" +
                             instance_guid() + "\"," +
                             kMountPhysicalMediaDisabledPolicySetting + "," +
                             "\"unmuteMicrophoneDisabled\":true}");
}

TEST_F(ArcPolicyBridgeTest, DefaultGeolocationSettingTest) {
  policy_map().Set(policy::key::kDefaultGeolocationSetting,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD, base::Value(1), nullptr);
  GetPoliciesAndVerifyResult("{\"apkCacheEnabled\":true,\"guid\":\"" +
                             instance_guid() + "\"," +
                             kMountPhysicalMediaDisabledPolicySetting + "," +
                             "\"shareLocationDisabled\":false}");
  policy_map().Set(policy::key::kDefaultGeolocationSetting,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD, base::Value(2), nullptr);
  GetPoliciesAndVerifyResult("{\"apkCacheEnabled\":true,\"guid\":\"" +
                             instance_guid() + "\"," +
                             kMountPhysicalMediaDisabledPolicySetting + "," +
                             "\"shareLocationDisabled\":true}");
  policy_map().Set(policy::key::kDefaultGeolocationSetting,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD, base::Value(3), nullptr);
  GetPoliciesAndVerifyResult("{\"apkCacheEnabled\":true,\"guid\":\"" +
                             instance_guid() + "\"," +
                             kMountPhysicalMediaDisabledPolicySetting + "," +
                             "\"shareLocationDisabled\":false}");
}

TEST_F(ArcPolicyBridgeTest, ExternalStorageDisabledTest) {
  policy_map().Set(policy::key::kExternalStorageDisabled,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD, base::Value(true), nullptr);
  GetPoliciesAndVerifyResult("{\"apkCacheEnabled\":true,\"guid\":\"" +
                             instance_guid() + "\"," +
                             kMountPhysicalMediaDisabledPolicySetting + "}");
}

TEST_F(ArcPolicyBridgeTest, WallpaperImageSetTest) {
  base::Value::Dict dict;
  dict.Set("url", "https://example.com/wallpaper.jpg");
  dict.Set("hash", "somehash");
  policy_map().Set(policy::key::kWallpaperImage, policy::POLICY_LEVEL_MANDATORY,
                   policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                   base::Value(std::move(dict)).Clone(), nullptr);
  GetPoliciesAndVerifyResult("{\"apkCacheEnabled\":true,\"guid\":\"" +
                             instance_guid() + "\"," +
                             kMountPhysicalMediaDisabledPolicySetting + "," +
                             "\"setWallpaperDisabled\":true}");
}

TEST_F(ArcPolicyBridgeTest, WallpaperImageSet_NotCompletePolicyTest) {
  base::Value::Dict dict;
  dict.Set("url", "https://example.com/wallpaper.jpg");
  // "hash" attribute is missing, so the policy shouldn't be set
  policy_map().Set(policy::key::kWallpaperImage, policy::POLICY_LEVEL_MANDATORY,
                   policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                   base::Value(std::move(dict)).Clone(), nullptr);
  GetPoliciesAndVerifyResult("{\"apkCacheEnabled\":true,\"guid\":\"" +
                             instance_guid() + "\"," +
                             kMountPhysicalMediaDisabledPolicySetting + "}");
}

TEST_F(ArcPolicyBridgeTest, CaCertificateTest) {
  // Enable CA certificates sync.
  policy_map().Set(policy::key::kArcCertificatesSyncMode,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD,
                   base::Value(ArcCertsSyncMode::COPY_CA_CERTS), nullptr);
  policy_map().Set(policy::key::kOpenNetworkConfiguration,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD, base::Value(kFakeONC), nullptr);
  GetPoliciesAndVerifyResult(
      "{\"apkCacheEnabled\":true,"
      "\"caCerts\":"
      "[{\"X509\":\"TWFuIGlzIGRpc3Rpbmd1aXNoZWQsIG5vdCBvbmx5IGJ5IGhpcyByZWFzb24"
      "sIGJ1dCBieSB0aGlzIHNpbmd1bGFyIHBhc3Npb24gZnJvbSBvdGhlciBhbmltYWxzLCB3aGl"
      "jaCBpcyBhIGx1c3Qgb2YgdGhlIG1pbmQsIHRoYXQgYnkgYSBwZXJzZXZlcmFuY2Ugb2YgZGV"
      "saWdodCBpbiB0aGUgY29udGludWVkIGFuZCBpbmRlZmF0aWdhYmxlIGdlbmVyYXRpb24gb2Y"
      "ga25vd2xlZGdlLCBleGNlZWRzIHRoZSBzaG9ydCB2ZWhlbWVuY2Ugb2YgYW55IGNhcm5hbCB"
      "wbGVhc3VyZS4=\"}"
      "],"
      "\"credentialsConfigDisabled\":true,"
      "\"guid\":\"" +
      instance_guid() + "\"," + kMountPhysicalMediaDisabledPolicySetting + "}");

  // Disable CA certificates sync.
  policy_map().Set(policy::key::kArcCertificatesSyncMode,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD,
                   base::Value(ArcCertsSyncMode::SYNC_DISABLED), nullptr);
  GetPoliciesAndVerifyResult("{\"apkCacheEnabled\":true,\"guid\":\"" +
                             instance_guid() + "\"," +
                             kMountPhysicalMediaDisabledPolicySetting + "}");
}

TEST_F(ArcPolicyBridgeTest, DeveloperToolsPolicyAllowedTest) {
  profile()->GetTestingPrefService()->SetManagedPref(
      ::prefs::kDevToolsAvailability,
      std::make_unique<base::Value>(static_cast<int>(
          policy::DeveloperToolsPolicyHandler::Availability::kAllowed)));
  GetPoliciesAndVerifyResult(
      "{\"apkCacheEnabled\":true,\"debuggingFeaturesDisabled\":false,"
      "\"guid\":\"" +
      instance_guid() + "\"," + kMountPhysicalMediaDisabledPolicySetting + "}");
}

TEST_F(ArcPolicyBridgeTest,
       DeveloperToolsPolicyDisallowedForForceInstalledExtensionsTest) {
  profile()->GetTestingPrefService()->SetManagedPref(
      ::prefs::kDevToolsAvailability,
      std::make_unique<base::Value>(
          static_cast<int>(policy::DeveloperToolsPolicyHandler::Availability::
                               kDisallowedForForceInstalledExtensions)));
  GetPoliciesAndVerifyResult(
      "{\"apkCacheEnabled\":true,\"debuggingFeaturesDisabled\":false,"
      "\"guid\":\"" +
      instance_guid() + "\"," + kMountPhysicalMediaDisabledPolicySetting + "}");
}

TEST_F(ArcPolicyBridgeTest, DeveloperToolsPolicyDisallowedTest) {
  profile()->GetTestingPrefService()->SetManagedPref(
      ::prefs::kDevToolsAvailability,
      std::make_unique<base::Value>(static_cast<int>(
          policy::DeveloperToolsPolicyHandler::Availability::kDisallowed)));
  GetPoliciesAndVerifyResult(
      "{\"apkCacheEnabled\":true,\"debuggingFeaturesDisabled\":true,"
      "\"guid\":\"" +
      instance_guid() + "\"," + kMountPhysicalMediaDisabledPolicySetting + "}");
}

TEST_F(ArcPolicyBridgeTest, ForceDevToolsAvailabilityTest) {
  profile()->GetTestingPrefService()->SetManagedPref(
      ::prefs::kDevToolsAvailability,
      std::make_unique<base::Value>(static_cast<int>(
          policy::DeveloperToolsPolicyHandler::Availability::kDisallowed)));
  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kForceDevToolsAvailable);
  GetPoliciesAndVerifyResult(
      "{\"apkCacheEnabled\":true,\"debuggingFeaturesDisabled\":false,"
      "\"guid\":\"" +
      instance_guid() + "\"," + kMountPhysicalMediaDisabledPolicySetting + "}");
  command_line.GetProcessCommandLine()->RemoveSwitch(
      switches::kForceDevToolsAvailable);
}

TEST_F(ArcPolicyBridgeTest, ManagedConfigurationVariablesTest) {
  policy_map().Set(
      policy::key::kArcPolicy, policy::POLICY_LEVEL_MANDATORY,
      policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
      base::Value(
          "{\"applications\":"
          "[{\"packageName\":\"de.blinkt.openvpn\","
          "\"installType\":\"REQUIRED\","
          "\"managedConfiguration\":"
          "{\"email\":\"${USER_EMAIL}\","
          "\"special_chars\":\"${`~!@#$%^&*(),_-+={[}}|\\\\:,;\\\"',>.?/{}\","
          "\"other_attribute\":\"untouched\"}"
          "}],"
          "\"defaultPermissionPolicy\":\"GRANT\"}"),
      nullptr);
  GetPoliciesAndVerifyResult(
      "{\"apkCacheEnabled\":true,\"applications\":"
      "[{\"installType\":\"REQUIRED\","
      "\"managedConfiguration\":"
      "{\"email\":\"user@gmail.com\","
      "\"other_attribute\":\"untouched\","
      "\"special_chars\":\"${`~!@#$%^&*(),_-+={[}}|\\\\:,;\\\"',>.?/{}\"},"
      "\"packageName\":\"de.blinkt.openvpn\""
      "}],"
      "\"defaultPermissionPolicy\":\"GRANT\","
      "\"guid\":\"" +
      instance_guid() + "\"," + kMountPhysicalMediaDisabledPolicySetting + "}");
}

TEST_F(ArcPolicyBridgeTest, MultiplePoliciesTest) {
  policy_map().Set(
      policy::key::kArcPolicy, policy::POLICY_LEVEL_MANDATORY,
      policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
      base::Value("{\"applications\":"
                  "[{\"packageName\":\"com.google.android.apps.youtube.kids\","
                  "\"installType\":\"REQUIRED\","
                  "\"lockTaskAllowed\":false,"
                  "\"permissionGrants\":[]"
                  "}],"
                  "\"defaultPermissionPolicy\":\"GRANT\"}"),
      nullptr);
  policy_map().Set(policy::key::kHomepageLocation,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD,
                   base::Value("http://chromium.org"), nullptr);
  policy_map().Set(policy::key::kVideoCaptureAllowed,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD, base::Value(false), nullptr);
  GetPoliciesAndVerifyResult(
      "{\"apkCacheEnabled\":true,\"applications\":"
      "[{\"installType\":\"REQUIRED\","
      "\"lockTaskAllowed\":false,"
      "\"packageName\":\"com.google.android.apps.youtube.kids\","
      "\"permissionGrants\":[]"
      "}],"
      "\"cameraDisabled\":true,"
      "\"defaultPermissionPolicy\":\"GRANT\","
      "\"guid\":\"" +
      instance_guid() + "\"," + kMountPhysicalMediaDisabledPolicySetting + "}");
}

TEST_F(ArcPolicyBridgeTest, EmptyReportComplianceTest) {
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcPolicyComplianceReported));
  ReportComplianceAndVerifyObserverCallback("{}");
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcPolicyComplianceReported));
}

TEST_F(ArcPolicyBridgeTest, ParsableReportComplianceTest) {
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcPolicyComplianceReported));
  ReportComplianceAndVerifyObserverCallback("{\"nonComplianceDetails\" : []}");
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcPolicyComplianceReported));
}

TEST_F(ArcPolicyBridgeTest, NonParsableReportComplianceTest) {
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcPolicyComplianceReported));
  ReportComplianceAndVerifyObserverCallback("\"nonComplianceDetails\" : [}");
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcPolicyComplianceReported));
}

TEST_F(ArcPolicyBridgeTest, ReportComplianceTest_WithNonCompliantDetails) {
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcPolicyComplianceReported));
  ReportComplianceAndVerifyObserverCallback(
      "{\"nonComplianceDetails\" : "
      "[{\"fieldPath\":\"\",\"nonComplianceReason\":0,\"packageName\":\"\","
      "\"settingName\":\"someSetting\",\"cachedSize\":-1},"
      "{\"cachedSize\":-1,\"fieldPath\":\"\",\"nonComplianceReason\":6,"
      "\"packageName\":\"\",\"settingName\":\"guid\"}]}");
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcPolicyComplianceReported));
}

TEST_F(ArcPolicyBridgeTest, ReportDPCVersionTest) {
  ReportDPCVersionAndVerifyObserverCallback("100");
}

// This and the following test send the policies through a mojo connection
// between a PolicyInstance and the PolicyBridge.
TEST_F(ArcPolicyBridgeTest, PolicyInstanceUnmanagedTest) {
  policy_bridge()->OverrideIsManagedForTesting(false);
  GetPoliciesAndVerifyResult("");
}

TEST_F(ArcPolicyBridgeTest, PolicyInstanceManagedTest) {
  GetPoliciesAndVerifyResult("{\"apkCacheEnabled\":true,\"guid\":\"" +
                             instance_guid() + "\"," +
                             kMountPhysicalMediaDisabledPolicySetting + "}");
}

TEST_F(ArcPolicyBridgeTest, VpnConfigAllowedTest) {
  policy_map().Set(policy::key::kVpnConfigAllowed,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD, base::Value(false), nullptr);
  GetPoliciesAndVerifyResult("{\"apkCacheEnabled\":true,\"guid\":\"" +
                             instance_guid() + "\"," +
                             kMountPhysicalMediaDisabledPolicySetting +
                             ",\"vpnConfigDisabled\":true}");
}

TEST_F(ArcPolicyBridgeTest, ManualChildUserPoliciesSet) {
  // Mark profile as supervised user.
  profile()->SetIsSupervisedProfile();
  EXPECT_TRUE(profile()->IsChild());

  policy_map().Set(policy::key::kArcPolicy, policy::POLICY_LEVEL_MANDATORY,
                   policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                   base::Value("{}"), /* external_data_fetcher */ nullptr);

  // Applications policy is not present so only playStoreMode policy is set.
  GetPoliciesAndVerifyResult(
      base::StrCat({"{\"apkCacheEnabled\":true,\"guid\":\"", instance_guid(),
                    "\",", kMountPhysicalMediaDisabledPolicySetting, ",",
                    kSupervisedUserPlayStoreModePolicySetting, "}"}));
}

TEST_P(ArcPolicyBridgeAffiliatedTest, ApkCacheEnabledTest) {
  const std::string expected_apk_cache_enabled_result(
      "{\"apkCacheEnabled\":true,\"guid\":\"" + instance_guid() + "\"," +
      kMountPhysicalMediaDisabledPolicySetting + "}");
  const std::string expected_apk_cache_disabled_result(
      "{\"apkCacheEnabled\":false,\"guid\":\"" + instance_guid() + "\"," +
      kMountPhysicalMediaDisabledPolicySetting + "}");

  const std::string arc_apk_cache_enabled_policy("{\"apkCacheEnabled\":true}");
  policy_map().Set(policy::key::kArcPolicy, policy::POLICY_LEVEL_MANDATORY,
                   policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                   base::Value(arc_apk_cache_enabled_policy), nullptr);
  GetPoliciesAndVerifyResultWithAffiliation(
      /* expected_policy_json_affiliated */ expected_apk_cache_enabled_result,
      /* expected_policy_json_not_affiliated */
      expected_apk_cache_disabled_result);

  const std::string arc_apk_cache_disabled_policy(
      "{\"apkCacheEnabled\":false}");
  policy_map().Set(policy::key::kArcPolicy, policy::POLICY_LEVEL_MANDATORY,
                   policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                   base::Value(arc_apk_cache_disabled_policy), nullptr);
  GetPoliciesAndVerifyResultWithAffiliation(
      /* expected_policy_json_affiliated */ expected_apk_cache_enabled_result,
      /* expected_policy_json_not_affiliated */
      expected_apk_cache_disabled_result);

  const std::string arc_apk_cache_no_policy("{}");
  policy_map().Set(policy::key::kArcPolicy, policy::POLICY_LEVEL_MANDATORY,
                   policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                   base::Value(arc_apk_cache_no_policy), nullptr);
  GetPoliciesAndVerifyResultWithAffiliation(
      /* expected_policy_json_affiliated */ expected_apk_cache_enabled_result,
      /* expected_policy_json_not_affiliated */
      expected_apk_cache_disabled_result);
}

// Boolean parameter means if user is affiliated on the device. Affiliated
// users belong to the domain that owns the device.
// Affiliated user should always have enabled APK cache; not affiliated user
// should always have it disabled.
INSTANTIATE_TEST_SUITE_P(ArcPolicyBridgeAffiliatedTestInstance,
                         ArcPolicyBridgeAffiliatedTest,
                         testing::Bool());

// Tests that if cert store service is non-null, the required key pair policy is
// set to the required certificate list.
TEST_F(ArcPolicyBridgeCertStoreTest, RequiredKeyPairsBasicTest) {
  // One certificate is required to be installed.
  cert_store_service()->set_required_cert_names_for_testing({kFakeCertName});
  GetPoliciesAndVerifyResult(base::StrCat(
      {"{\"apkCacheEnabled\":true,\"guid\":\"", instance_guid(), "\",",
       kMountPhysicalMediaDisabledPolicySetting, ",",
       base::StringPrintf(kRequiredKeyPairsFormat, kFakeCertName), "}"}));

  // An empty list is required to be installed.
  cert_store_service()->set_required_cert_names_for_testing({});
  GetPoliciesAndVerifyResult(
      base::StrCat({"{\"apkCacheEnabled\":true,\"guid\":\"", instance_guid(),
                    "\",", kMountPhysicalMediaDisabledPolicySetting, ",",
                    kRequiredKeyPairsEmpty, "}"}));
}

// Tests that if cert store service is non-null, corporate usage key exists and
// available to ARC app, ChoosePrivateKeyRules policy is propagated correctly.
TEST_F(ArcPolicyBridgeCertStoreTest, KeyPermissionsBasicTest) {
  EXPECT_TRUE(cert_store_service());

  // One certificate is required to be installed.
  cert_store_service()->set_required_cert_names_for_testing({kFakeCertName});

  base::Value::Dict key_permissions;
  AddKeyPermissionForAppId(key_permissions, kAndroidAppId, true /* allowed */);
  AddKeyPermissionForAppId(key_permissions, kChromeAppId, true /* allowed */);

  policy_map().Set(policy::key::kKeyPermissions, policy::POLICY_LEVEL_MANDATORY,
                   policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                   base::Value(std::move(key_permissions)),
                   /* external_data_fetcher */ nullptr);
  GetPoliciesAndVerifyResult(base::StrCat(
      {"{\"apkCacheEnabled\":true,",
       base::StringPrintf(kChoosePrivateKeyRulesFormat, kAndroidAppId,
                          kFakeCertName),
       ",\"guid\":\"", instance_guid(), "\",",
       kMountPhysicalMediaDisabledPolicySetting, ",",
       "\"privateKeySelectionEnabled\":true,",
       base::StringPrintf(kRequiredKeyPairsFormat, kFakeCertName), "}"}));
}

// Tests that if cert store service is non-null, corporate usage key exists and
// not to any ARC apps, ChoosePrivateKeyRules policy is not set.
TEST_F(ArcPolicyBridgeCertStoreTest, KeyPermissionsEmptyTest) {
  base::Value::Dict key_permissions;
  AddKeyPermissionForAppId(key_permissions, kAndroidAppId, false /* allowed */);
  AddKeyPermissionForAppId(key_permissions, kChromeAppId, true /* allowed */);

  // One certificate is required to be installed.
  cert_store_service()->set_required_cert_names_for_testing({kFakeCertName});

  policy_map().Set(policy::key::kKeyPermissions, policy::POLICY_LEVEL_MANDATORY,
                   policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                   base::Value(std::move(key_permissions)),
                   /* external_data_fetcher */ nullptr);
  GetPoliciesAndVerifyResult(base::StrCat(
      {"{\"apkCacheEnabled\":true,\"guid\":\"", instance_guid(), "\",",
       kMountPhysicalMediaDisabledPolicySetting, ",",
       base::StringPrintf(kRequiredKeyPairsFormat, kFakeCertName), "}"}));
}

// Tests that if cert store service is non-null, corporate usage keys do not
// exist, but in theory are available to ARC apps, ChoosePrivateKeyRules policy
// is not set.
TEST_F(ArcPolicyBridgeCertStoreTest, KeyPermissionsNoCertsTest) {
  base::Value::Dict key_permissions;
  AddKeyPermissionForAppId(key_permissions, kAndroidAppId, true /* allowed */);
  AddKeyPermissionForAppId(key_permissions, kChromeAppId, true /* allowed */);

  cert_store_service()->set_required_cert_names_for_testing({});

  policy_map().Set(policy::key::kKeyPermissions, policy::POLICY_LEVEL_MANDATORY,
                   policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                   base::Value(std::move(key_permissions)),
                   /* external_data_fetcher */ nullptr);
  GetPoliciesAndVerifyResult(
      base::StrCat({"{\"apkCacheEnabled\":true,\"guid\":\"", instance_guid(),
                    "\",", kMountPhysicalMediaDisabledPolicySetting, ",",
                    kRequiredKeyPairsEmpty, "}"}));
}

TEST_F(ArcPolicyBridgeTest, ConfigureRevenPoliciesTest) {
  base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();
  command_line.AppendSwitch(ash::switches::kRevenBranding);

  policy_map().Set(
      policy::key::kArcPolicy, policy::POLICY_LEVEL_MANDATORY,
      policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
      base::Value("{\"applications\":"
                  "["
                  "{\"packageName\":\"com.google.android.apps.youtube.kids\","
                  "\"installType\":\"REQUIRED\","
                  "\"lockTaskAllowed\":false,"
                  "\"permissionGrants\":[]"
                  "},"
                  "{\"packageName\":\"com.zimperium.zips\","
                  "\"installType\":\"REQUIRED\","
                  "\"lockTaskAllowed\":false,"
                  "\"permissionGrants\":[]"
                  "}"
                  "],"
                  "\"defaultPermissionPolicy\":\"GRANT\"}"),
      nullptr);

  GetPoliciesAndVerifyResult(
      "{\"apkCacheEnabled\":true,\"applications\":"
      "[{\"installType\":\"REQUIRED\","
      "\"lockTaskAllowed\":false,"
      "\"packageName\":\"com.zimperium.zips\","
      "\"permissionGrants\":[]"
      "}],"
      "\"defaultPermissionPolicy\":\"GRANT\","
      "\"guid\":\"" +
      instance_guid() + "\"," + kMountPhysicalMediaDisabledPolicySetting + "," +
      "\"playStoreMode\":\"WHITELIST\"" + "}"); // nocheck
}

}  // namespace arc
