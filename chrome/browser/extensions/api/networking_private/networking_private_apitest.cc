// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/onc/onc_constants.h"
#include "content/public/test/browser_test.h"
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
const char kOnline[] = "Online";
const char kGuid[] = "SOME_GUID";

class TestNetworkingPrivateDelegate : public NetworkingPrivateDelegate {
 public:
  explicit TestNetworkingPrivateDelegate(bool test_failure)
      : fail_(test_failure) {}

  TestNetworkingPrivateDelegate(const TestNetworkingPrivateDelegate&) = delete;
  TestNetworkingPrivateDelegate& operator=(
      const TestNetworkingPrivateDelegate&) = delete;

  ~TestNetworkingPrivateDelegate() override = default;

  // Asynchronous methods
  void GetProperties(const std::string& guid,
                     PropertiesCallback callback) override {
    ValueResult(guid, std::move(callback));
  }

  void GetManagedProperties(const std::string& guid,
                            PropertiesCallback callback) override {
    ValueResult(guid, std::move(callback));
  }

  void GetState(const std::string& guid,
                DictionaryCallback success_callback,
                FailureCallback failure_callback) override {
    DictionaryResult(guid, std::move(success_callback),
                     std::move(failure_callback));
  }

  void SetProperties(const std::string& guid,
                     base::Value::Dict properties,
                     bool allow_set_shared_config,
                     VoidCallback success_callback,
                     FailureCallback failure_callback) override {
    VoidResult(std::move(success_callback), std::move(failure_callback));
  }

  void CreateNetwork(bool shared,
                     base::Value::Dict properties,
                     StringCallback success_callback,
                     FailureCallback failure_callback) override {
    StringResult(std::move(success_callback), std::move(failure_callback),
                 kSuccess);
  }

  void ForgetNetwork(const std::string& guid,
                     bool allow_forget_shared_network,
                     VoidCallback success_callback,
                     FailureCallback failure_callback) override {
    VoidResult(std::move(success_callback), std::move(failure_callback));
  }

  void GetNetworks(const std::string& network_type,
                   bool configured_only,
                   bool visible_only,
                   int limit,
                   NetworkListCallback success_callback,
                   FailureCallback failure_callback) override {
    if (fail_) {
      std::move(failure_callback).Run(kFailure);
    } else {
      base::Value::List result;
      base::Value::Dict network;
      network.Set(::onc::network_config::kType,
                  ::onc::network_config::kEthernet);
      network.Set(::onc::network_config::kGUID, kGuid);
      result.Append(std::move(network));
      std::move(success_callback).Run(std::move(result));
    }
  }

  void StartConnect(const std::string& guid,
                    VoidCallback success_callback,
                    FailureCallback failure_callback) override {
    VoidResult(std::move(success_callback), std::move(failure_callback));
  }

  void StartDisconnect(const std::string& guid,
                       VoidCallback success_callback,
                       FailureCallback failure_callback) override {
    VoidResult(std::move(success_callback), std::move(failure_callback));
  }

  void StartActivate(const std::string& guid,
                     const std::string& carrier,
                     VoidCallback success_callback,
                     FailureCallback failure_callback) override {
    VoidResult(std::move(success_callback), std::move(failure_callback));
  }

  void GetCaptivePortalStatus(const std::string& guid,
                              StringCallback success_callback,
                              FailureCallback failure_callback) override {
    StringResult(std::move(success_callback), std::move(failure_callback),
                 kOnline);
  }

  void UnlockCellularSim(const std::string& guid,
                         const std::string& pin,
                         const std::string& puk,
                         VoidCallback success_callback,
                         FailureCallback failure_callback) override {
    VoidResult(std::move(success_callback), std::move(failure_callback));
  }

  void SetCellularSimState(const std::string& guid,
                           bool require_pin,
                           const std::string& current_pin,
                           const std::string& new_pin,
                           VoidCallback success_callback,
                           FailureCallback failure_callback) override {
    VoidResult(std::move(success_callback), std::move(failure_callback));
  }

  void SelectCellularMobileNetwork(const std::string& guid,
                                   const std::string& nework_id,
                                   VoidCallback success_callback,
                                   FailureCallback failure_callback) override {
    VoidResult(std::move(success_callback), std::move(failure_callback));
  }

  void GetEnabledNetworkTypes(EnabledNetworkTypesCallback callback) override {
    base::Value::List result;
    if (!fail_) {
      result.Append(::onc::network_config::kEthernet);
    }
    std::move(callback).Run(std::move(result));
  }

  void GetDeviceStateList(DeviceStateListCallback callback) override {
    DeviceStateList result;
    if (!fail_) {
      api::networking_private::DeviceStateProperties& properties =
          result.emplace_back();
      properties.type = api::networking_private::NetworkType::kEthernet;
      properties.state = api::networking_private::DeviceStateType::kEnabled;
    }
    std::move(callback).Run(std::move(result));
  }

  void GetGlobalPolicy(GetGlobalPolicyCallback callback) override {
    std::move(callback).Run(base::Value::Dict());
  }

  void GetCertificateLists(GetCertificateListsCallback callback) override {
    std::move(callback).Run(base::Value::Dict());
  }

  // Synchronous methods
  void EnableNetworkType(const std::string& type,
                         BoolCallback callback) override {
    enabled_[type] = true;
    std::move(callback).Run(!fail_);
  }

  void DisableNetworkType(const std::string& type,
                          BoolCallback callback) override {
    disabled_[type] = true;
    std::move(callback).Run(!fail_);
  }

  void RequestScan(const std::string& type, BoolCallback callback) override {
    scan_requested_.push_back(type);
    std::move(callback).Run(!fail_);
  }

  void set_fail(bool fail) { fail_ = fail; }
  bool GetEnabled(const std::string& type) { return enabled_[type]; }
  bool GetDisabled(const std::string& type) { return disabled_[type]; }
  const std::vector<std::string>& GetScanRequested() { return scan_requested_; }

  void DictionaryResult(const std::string& guid,
                        DictionaryCallback success_callback,
                        FailureCallback failure_callback) {
    if (fail_) {
      std::move(failure_callback).Run(kFailure);
    } else {
      base::Value::Dict result;
      result.Set(::onc::network_config::kGUID, guid);
      result.Set(::onc::network_config::kType, ::onc::network_config::kWiFi);
      std::move(success_callback).Run(std::move(result));
    }
  }

  void StringResult(StringCallback success_callback,
                    FailureCallback failure_callback,
                    const std::string& result) {
    if (fail_) {
      std::move(failure_callback).Run(kFailure);
    } else {
      std::move(success_callback).Run(result);
    }
  }

  void BoolResult(BoolCallback success_callback,
                  FailureCallback failure_callback) {
    if (fail_) {
      std::move(failure_callback).Run(kFailure);
    } else {
      std::move(success_callback).Run(true);
    }
  }

  void VoidResult(VoidCallback success_callback,
                  FailureCallback failure_callback) {
    if (fail_) {
      std::move(failure_callback).Run(kFailure);
    } else {
      std::move(success_callback).Run();
    }
  }

  void ValueResult(const std::string& guid, PropertiesCallback callback) {
    if (fail_) {
      std::move(callback).Run(std::nullopt, kFailure);
      return;
    }
    base::Value::Dict result;
    result.Set(::onc::network_config::kGUID, guid);
    result.Set(::onc::network_config::kType, ::onc::network_config::kWiFi);
    std::move(callback).Run(std::move(result), std::nullopt);
  }

 private:
  bool fail_;
  std::map<std::string, bool> enabled_;
  std::map<std::string, bool> disabled_;
  std::vector<std::string> scan_requested_;
};

class NetworkingPrivateApiTest : public ExtensionApiTest {
 public:
  NetworkingPrivateApiTest() = default;

  NetworkingPrivateApiTest(const NetworkingPrivateApiTest&) = delete;
  NetworkingPrivateApiTest& operator=(const NetworkingPrivateApiTest&) = delete;

  ~NetworkingPrivateApiTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    // Allowlist the extension ID of the test extension.
    command_line->AppendSwitchASCII(
        extensions::switches::kAllowlistedExtensionID,
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
    const std::string extension_url = "main.html?" + subtest;
    return RunExtensionTest("networking_private",
                            {.extension_url = extension_url.c_str()},
                            {.load_as_component = true});
  }

 private:
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
};

}  // namespace

// Place each subtest into a separate browser test so that the stub networking
// library state is reset for each subtest run. This way they won't affect each
// other. TODO(stevenjb): Use extensions::ApiUnitTest once moved to
// src/extensions.

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

namespace {

// Test failure case
class NetworkingPrivateApiTestFail : public NetworkingPrivateApiTest {
 public:
  NetworkingPrivateApiTestFail() { test_failure_ = true; }

  NetworkingPrivateApiTestFail(const NetworkingPrivateApiTestFail&) = delete;
  NetworkingPrivateApiTestFail& operator=(const NetworkingPrivateApiTestFail&) =
      delete;

  ~NetworkingPrivateApiTestFail() override = default;

 protected:
};

}  // namespace

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

}  // namespace extensions
