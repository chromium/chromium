// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/networking_cast_private/chrome_networking_cast_private_delegate.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/onc/onc_constants.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/networking_private/networking_private_delegate.h"
#include "extensions/browser/api/networking_private/networking_private_delegate_factory.h"
#include "extensions/common/switches.h"

namespace extensions {

// This tests just the interface for the networkingPrivate API, i.e. it ensures
// that the delegate methods are called as expected.

// The implementations (which differ significantly between chromeos and
// windows/mac) are tested independently in
// networking_private_[chromeos|service_client]_apitest.cc.
// See also crbug.com/460119.

namespace {

const char kFailure[] = "Failure";
const char kSuccess[] = "Success";
const char kGuid[] = "SOME_GUID";

class TestNetworkingPrivateDelegate : public NetworkingPrivateDelegate {
 public:
  explicit TestNetworkingPrivateDelegate(bool test_failure)
      : fail_(test_failure) {}

  ~TestNetworkingPrivateDelegate() override {}

  // Asynchronous methods
  void GetProperties(const std::string& guid,
                     const DictionaryCallback& success_callback,
                     const FailureCallback& failure_callback) override {
    DictionaryResult(guid, success_callback, failure_callback);
  }

  void GetManagedProperties(const std::string& guid,
                            const DictionaryCallback& success_callback,
                            const FailureCallback& failure_callback) override {
    DictionaryResult(guid, success_callback, failure_callback);
  }

  void GetState(const std::string& guid,
                const DictionaryCallback& success_callback,
                const FailureCallback& failure_callback) override {
    DictionaryResult(guid, success_callback, failure_callback);
  }

  void SetProperties(const std::string& guid,
                     std::unique_ptr<base::DictionaryValue> properties,
                     bool allow_set_shared_config,
                     const VoidCallback& success_callback,
                     const FailureCallback& failure_callback) override {
    VoidResult(success_callback, failure_callback);
  }

  void CreateNetwork(bool shared,
                     std::unique_ptr<base::DictionaryValue> properties,
                     const StringCallback& success_callback,
                     const FailureCallback& failure_callback) override {
    StringResult(success_callback, failure_callback);
  }

  void ForgetNetwork(const std::string& guid,
                     bool allow_forget_shared_network,
                     const VoidCallback& success_callback,
                     const FailureCallback& failure_callback) override {
    VoidResult(success_callback, failure_callback);
  }

  void GetNetworks(const std::string& network_type,
                   bool configured_only,
                   bool visible_only,
                   int limit,
                   const NetworkListCallback& success_callback,
                   const FailureCallback& failure_callback) override {
    if (fail_) {
      failure_callback.Run(kFailure);
    } else {
      std::unique_ptr<base::ListValue> result(new base::ListValue);
      std::unique_ptr<base::DictionaryValue> network(new base::DictionaryValue);
      network->SetString(::onc::network_config::kType,
                         ::onc::network_config::kEthernet);
      network->SetString(::onc::network_config::kGUID, kGuid);
      result->Append(std::move(network));
      success_callback.Run(std::move(result));
    }
  }

  void StartConnect(const std::string& guid,
                    const VoidCallback& success_callback,
                    const FailureCallback& failure_callback) override {
    VoidResult(success_callback, failure_callback);
  }

  void StartDisconnect(const std::string& guid,
                       const VoidCallback& success_callback,
                       const FailureCallback& failure_callback) override {
    VoidResult(success_callback, failure_callback);
  }

  void StartActivate(const std::string& guid,
                     const std::string& carrier,
                     const VoidCallback& success_callback,
                     const FailureCallback& failure_callback) override {
    VoidResult(success_callback, failure_callback);
  }

  void SetWifiTDLSEnabledState(
      const std::string& ip_or_mac_address,
      bool enabled,
      const StringCallback& success_callback,
      const FailureCallback& failure_callback) override {
    StringResult(success_callback, failure_callback);
  }

  void GetWifiTDLSStatus(const std::string& ip_or_mac_address,
                         const StringCallback& success_callback,
                         const FailureCallback& failure_callback) override {
    StringResult(success_callback, failure_callback);
  }

  void GetCaptivePortalStatus(
      const std::string& guid,
      const StringCallback& success_callback,
      const FailureCallback& failure_callback) override {
    StringResult(success_callback, failure_callback);
  }

  void UnlockCellularSim(const std::string& guid,
                         const std::string& pin,
                         const std::string& puk,
                         const VoidCallback& success_callback,
                         const FailureCallback& failure_callback) override {
    VoidResult(success_callback, failure_callback);
  }

  void SetCellularSimState(const std::string& guid,
                           bool require_pin,
                           const std::string& current_pin,
                           const std::string& new_pin,
                           const VoidCallback& success_callback,
                           const FailureCallback& failure_callback) override {
    VoidResult(success_callback, failure_callback);
  }

  void SelectCellularMobileNetwork(
      const std::string& guid,
      const std::string& nework_id,
      const VoidCallback& success_callback,
      const FailureCallback& failure_callback) override {
    VoidResult(success_callback, failure_callback);
  }

  // Synchronous methods
  std::unique_ptr<base::ListValue> GetEnabledNetworkTypes() override {
    std::unique_ptr<base::ListValue> result;
    if (!fail_) {
      result.reset(new base::ListValue);
      result->AppendString(::onc::network_config::kEthernet);
    }
    return result;
  }

  std::unique_ptr<DeviceStateList> GetDeviceStateList() override {
    std::unique_ptr<DeviceStateList> result;
    if (fail_)
      return result;
    result.reset(new DeviceStateList);
    std::unique_ptr<api::networking_private::DeviceStateProperties> properties(
        new api::networking_private::DeviceStateProperties);
    properties->type = api::networking_private::NETWORK_TYPE_ETHERNET;
    properties->state = api::networking_private::DEVICE_STATE_TYPE_ENABLED;
    result->push_back(std::move(properties));
    return result;
  }

  std::unique_ptr<base::DictionaryValue> GetGlobalPolicy() override {
    return std::make_unique<base::DictionaryValue>();
  }

  std::unique_ptr<base::DictionaryValue> GetCertificateLists() override {
    return std::make_unique<base::DictionaryValue>();
  }

  bool EnableNetworkType(const std::string& type) override {
    enabled_[type] = true;
    return !fail_;
  }

  bool DisableNetworkType(const std::string& type) override {
    disabled_[type] = true;
    return !fail_;
  }

  bool RequestScan(const std::string& type) override {
    scan_requested_.push_back(type);
    return !fail_;
  }

  void set_fail(bool fail) { fail_ = fail; }
  bool GetEnabled(const std::string& type) { return enabled_[type]; }
  bool GetDisabled(const std::string& type) { return disabled_[type]; }
  const std::vector<std::string>& GetScanRequested() { return scan_requested_; }

  void DictionaryResult(const std::string& guid,
                        const DictionaryCallback& success_callback,
                        const FailureCallback& failure_callback) {
    if (fail_) {
      failure_callback.Run(kFailure);
    } else {
      std::unique_ptr<base::DictionaryValue> result(new base::DictionaryValue);
      result->SetString(::onc::network_config::kGUID, guid);
      result->SetString(::onc::network_config::kType,
                        ::onc::network_config::kWiFi);
      success_callback.Run(std::move(result));
    }
  }

  void StringResult(const StringCallback& success_callback,
                    const FailureCallback& failure_callback) {
    if (fail_) {
      failure_callback.Run(kFailure);
    } else {
      success_callback.Run(kSuccess);
    }
  }

  void BoolResult(const BoolCallback& success_callback,
                  const FailureCallback& failure_callback) {
    if (fail_) {
      failure_callback.Run(kFailure);
    } else {
      success_callback.Run(true);
    }
  }

  void VoidResult(const VoidCallback& success_callback,
                  const FailureCallback& failure_callback) {
    if (fail_) {
      failure_callback.Run(kFailure);
    } else {
      success_callback.Run();
    }
  }

 private:
  bool fail_;
  std::map<std::string, bool> enabled_;
  std::map<std::string, bool> disabled_;
  std::vector<std::string> scan_requested_;

  DISALLOW_COPY_AND_ASSIGN(TestNetworkingPrivateDelegate);
};

class TestNetworkingCastPrivateDelegate
    : public ChromeNetworkingCastPrivateDelegate {
 public:
  explicit TestNetworkingCastPrivateDelegate(bool test_failure)
      : fail_(test_failure) {}

  ~TestNetworkingCastPrivateDelegate() override {}

  void VerifyDestination(std::unique_ptr<Credentials> credentials,
                         const VerifiedCallback& success_callback,
                         const FailureCallback& failure_callback) override {
    if (fail_) {
      failure_callback.Run(kFailure);
    } else {
      success_callback.Run(true);
    }
  }

  void VerifyAndEncryptCredentials(
      const std::string& guid,
      std::unique_ptr<Credentials> credentials,
      const DataCallback& success_callback,
      const FailureCallback& failure_callback) override {
    if (fail_) {
      failure_callback.Run(kFailure);
    } else {
      success_callback.Run("encrypted_credentials");
    }
  }
  void VerifyAndEncryptData(const std::string& data,
                            std::unique_ptr<Credentials> credentials,
                            const DataCallback& success_callback,
                            const FailureCallback& failure_callback) override {
    if (fail_) {
      failure_callback.Run(kFailure);
    } else {
      success_callback.Run("encrypted_data");
    }
  }

 private:
  bool fail_;

  DISALLOW_COPY_AND_ASSIGN(TestNetworkingCastPrivateDelegate);
};

class NetworkingPrivateApiTest : public ExtensionApiTest {
 public:
  NetworkingPrivateApiTest() = default;
  ~NetworkingPrivateApiTest() override = default;

  void SetUp() override {
    networking_cast_delegate_factory_ = base::Bind(
        &NetworkingPrivateApiTest::CreateTestNetworkingCastPrivateDelegate,
        base::Unretained(this), test_failure_);
    ChromeNetworkingCastPrivateDelegate::SetFactoryCallbackForTest(
        &networking_cast_delegate_factory_);

    ExtensionApiTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    // Whitelist the extension ID of the test extension.
    command_line->AppendSwitchASCII(
        extensions::switches::kWhitelistedExtensionID,
        "epcifkihnkjgphfkloaaleeakhpmgdmn");
  }

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    NetworkingPrivateDelegateFactory::GetInstance()->SetTestingFactory(
        profile(),
        base::BindRepeating(
            &NetworkingPrivateApiTest::CreateTestNetworkingPrivateDelegate,
            base::Unretained(this), test_failure_));
  }

  void TearDown() override {
    ExtensionApiTest::TearDown();

    ChromeNetworkingCastPrivateDelegate::SetFactoryCallbackForTest(nullptr);
  }

  bool GetEnabled(const std::string& type) {
    return networking_private_delegate()->GetEnabled(type);
  }

  bool GetDisabled(const std::string& type) {
    return networking_private_delegate()->GetDisabled(type);
  }

  const std::vector<std::string>& GetScanRequested() {
    return networking_private_delegate()->GetScanRequested();
  }

 protected:
  bool RunNetworkingSubtest(const std::string& subtest) {
    return RunExtensionSubtest("networking_private",
                               "main.html?" + subtest,
                               kFlagEnableFileAccess | kFlagLoadAsComponent);
  }

 private:
  std::unique_ptr<ChromeNetworkingCastPrivateDelegate>
  CreateTestNetworkingCastPrivateDelegate(bool test_failure) {
    return std::make_unique<TestNetworkingCastPrivateDelegate>(test_failure);
  }

  std::unique_ptr<KeyedService> CreateTestNetworkingPrivateDelegate(
      bool test_failure,
      content::BrowserContext* /*context*/) {
    return std::make_unique<TestNetworkingPrivateDelegate>(test_failure);
  }

  // Returns a pointer to a networking private delegate created by the
  // test factory callback.
  TestNetworkingPrivateDelegate* networking_private_delegate() {
    return static_cast<TestNetworkingPrivateDelegate*>(
        NetworkingPrivateDelegateFactory::GetInstance()->GetForBrowserContext(
            profile()));
  }

 protected:
  bool test_failure_ = false;

 private:
  ChromeNetworkingCastPrivateDelegate::FactoryCallback
      networking_cast_delegate_factory_;

  DISALLOW_COPY_AND_ASSIGN(NetworkingPrivateApiTest);
};

}  // namespace

// Place each subtest into a separate browser test so that the stub networking
// library state is reset for each subtest run. This way they won't affect each
// other. TODO(stevenjb): Use extensions::ApiUnitTest once moved to
// src/extensions.

// These fail on Windows due to crbug.com/177163. Note: we still have partial
// coverage in NetworkingPrivateServiceClientApiTest. TODO(stevenjb): Enable
// these on Windows once we switch to extensions::ApiUnitTest.

#if !defined(OS_WIN)

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTest, GetProperties) {
  EXPECT_TRUE(RunNetworkingSubtest("getProperties")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTest, GetManagedProperties) {
  EXPECT_TRUE(RunNetworkingSubtest("getManagedProperties")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTest, GetState) {
  EXPECT_TRUE(RunNetworkingSubtest("getState")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTest, SetProperties) {
  EXPECT_TRUE(RunNetworkingSubtest("setProperties")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTest, CreateNetwork) {
  EXPECT_TRUE(RunNetworkingSubtest("createNetwork")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTest, ForgetNetwork) {
  EXPECT_TRUE(RunNetworkingSubtest("forgetNetwork")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTest, GetNetworks) {
  EXPECT_TRUE(RunNetworkingSubtest("getNetworks")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTest, GetVisibleNetworks) {
  EXPECT_TRUE(RunNetworkingSubtest("getVisibleNetworks")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTest, GetEnabledNetworkTypes) {
  EXPECT_TRUE(RunNetworkingSubtest("getEnabledNetworkTypes")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTest, GetDeviceStates) {
  EXPECT_TRUE(RunNetworkingSubtest("getDeviceStates")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTest, EnableNetworkType) {
  EXPECT_TRUE(RunNetworkingSubtest("enableNetworkType")) << message_;
  EXPECT_TRUE(GetEnabled(::onc::network_config::kEthernet));
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTest, DisableNetworkType) {
  EXPECT_TRUE(RunNetworkingSubtest("disableNetworkType")) << message_;
  EXPECT_TRUE(GetDisabled(::onc::network_config::kEthernet));
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTest, RequestNetworkScan) {
  EXPECT_TRUE(RunNetworkingSubtest("requestNetworkScan")) << message_;
  const std::vector<std::string>& scan_requested = GetScanRequested();
  ASSERT_EQ(2u, scan_requested.size());
  EXPECT_EQ("", scan_requested[0]);
  EXPECT_EQ("Cellular", scan_requested[1]);
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTest, StartConnect) {
  EXPECT_TRUE(RunNetworkingSubtest("startConnect")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTest, StartDisconnect) {
  EXPECT_TRUE(RunNetworkingSubtest("startDisconnect")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTest, StartActivate) {
  EXPECT_TRUE(RunNetworkingSubtest("startActivate")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTest, VerifyDestination) {
  EXPECT_TRUE(RunNetworkingSubtest("verifyDestination")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTest, VerifyAndEncryptCredentials) {
  EXPECT_TRUE(RunNetworkingSubtest("verifyAndEncryptCredentials")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTest, VerifyAndEncryptData) {
  EXPECT_TRUE(RunNetworkingSubtest("verifyAndEncryptData")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTest, SetWifiTDLSEnabledState) {
  EXPECT_TRUE(RunNetworkingSubtest("setWifiTDLSEnabledState")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTest, GetWifiTDLSStatus) {
  EXPECT_TRUE(RunNetworkingSubtest("getWifiTDLSStatus")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTest, GetCaptivePortalStatus) {
  EXPECT_TRUE(RunNetworkingSubtest("getCaptivePortalStatus")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTest, UnlockCellularSim) {
  EXPECT_TRUE(RunNetworkingSubtest("unlockCellularSim")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTest, SetCellularSimState) {
  EXPECT_TRUE(RunNetworkingSubtest("setCellularSimState")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTest, SelectCellularMobileNetwork) {
  EXPECT_TRUE(RunNetworkingSubtest("selectCellularMobileNetwork")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTest, GetGlobalPolicy) {
  EXPECT_TRUE(RunNetworkingSubtest("getGlobalPolicy")) << message_;
}

// Test failure case

class NetworkingPrivateApiTestFail : public NetworkingPrivateApiTest {
 public:
  NetworkingPrivateApiTestFail() { test_failure_ = true; }

  ~NetworkingPrivateApiTestFail() override = default;

 protected:
  DISALLOW_COPY_AND_ASSIGN(NetworkingPrivateApiTestFail);
};

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTestFail, GetProperties) {
  EXPECT_FALSE(RunNetworkingSubtest("getProperties")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTestFail, GetManagedProperties) {
  EXPECT_FALSE(RunNetworkingSubtest("getManagedProperties")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTestFail, GetState) {
  EXPECT_FALSE(RunNetworkingSubtest("getState")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTestFail, SetProperties) {
  EXPECT_FALSE(RunNetworkingSubtest("setProperties")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTestFail, CreateNetwork) {
  EXPECT_FALSE(RunNetworkingSubtest("createNetwork")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTestFail, ForgetNetwork) {
  EXPECT_FALSE(RunNetworkingSubtest("forgetNetwork")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTestFail, GetNetworks) {
  EXPECT_FALSE(RunNetworkingSubtest("getNetworks")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTestFail, GetVisibleNetworks) {
  EXPECT_FALSE(RunNetworkingSubtest("getVisibleNetworks")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTestFail, GetEnabledNetworkTypes) {
  EXPECT_FALSE(RunNetworkingSubtest("getEnabledNetworkTypes")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTestFail, GetDeviceStates) {
  EXPECT_FALSE(RunNetworkingSubtest("getDeviceStates")) << message_;
}

// Note: Synchronous methods never fail:
// * disableNetworkType
// * enableNetworkType
// * requestNetworkScan
// * getGlobalPolicy

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTestFail, StartConnect) {
  EXPECT_FALSE(RunNetworkingSubtest("startConnect")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTestFail, StartDisconnect) {
  EXPECT_FALSE(RunNetworkingSubtest("startDisconnect")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTestFail, StartActivate) {
  EXPECT_FALSE(RunNetworkingSubtest("startActivate")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTestFail, VerifyDestination) {
  EXPECT_FALSE(RunNetworkingSubtest("verifyDestination")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTestFail,
                       VerifyAndEncryptCredentials) {
  EXPECT_FALSE(RunNetworkingSubtest("verifyAndEncryptCredentials")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTestFail, VerifyAndEncryptData) {
  EXPECT_FALSE(RunNetworkingSubtest("verifyAndEncryptData")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTestFail, SetWifiTDLSEnabledState) {
  EXPECT_FALSE(RunNetworkingSubtest("setWifiTDLSEnabledState")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTestFail, GetWifiTDLSStatus) {
  EXPECT_FALSE(RunNetworkingSubtest("getWifiTDLSStatus")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTestFail, GetCaptivePortalStatus) {
  EXPECT_FALSE(RunNetworkingSubtest("getCaptivePortalStatus")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTestFail, UnlockCellularSim) {
  EXPECT_FALSE(RunNetworkingSubtest("unlockCellularSim")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTestFail, SetCellularSimState) {
  EXPECT_FALSE(RunNetworkingSubtest("setCellularSimState")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTestFail,
                       SelectCellularMobileNetwork) {
  EXPECT_FALSE(RunNetworkingSubtest("selectCellularMobileNetwork")) << message_;
}

#endif // defined(OS_WIN)

}  // namespace extensions
