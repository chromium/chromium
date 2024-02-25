// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_NETWORK_DIAGNOSTICS_TEST_HELPER_H_
#define CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_NETWORK_DIAGNOSTICS_TEST_HELPER_H_

#include <memory>

#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/services/network_config/cros_network_config.h"
#include "components/prefs/testing_pref_service.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace network_diagnostics {

class NetworkDiagnosticsTestHelper : public ::testing::Test {
 public:
  NetworkDiagnosticsTestHelper();
  NetworkDiagnosticsTestHelper(const NetworkDiagnosticsTestHelper&) = delete;
  NetworkDiagnosticsTestHelper& operator=(const NetworkDiagnosticsTestHelper&) =
      delete;
  ~NetworkDiagnosticsTestHelper() override;

  // Sets up the WiFi configuration for tests.
  void SetUpWiFi(const char* state);

 protected:
  std::string ConfigureService(const std::string& shill_json_string);
  void SetServiceProperty(const std::string& service_path,
                          const std::string& key,
                          const base::Value& value);
  NetworkHandlerTestHelper* helper() { return helper_.get(); }
  const std::string& wifi_path() { return wifi_path_; }

 private:
  // Member order declaration done in a way so that members outlive those that
  // are dependent on them.
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<NetworkHandlerTestHelper> helper_;
  std::unique_ptr<network_config::CrosNetworkConfig> cros_network_config_;
  sync_preferences::TestingPrefServiceSyncable user_prefs_;
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  std::string wifi_path_;
  std::string wifi_guid_;
  system::ScopedFakeStatisticsProvider statistics_provider_;
};

}  // namespace network_diagnostics
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_NETWORK_DIAGNOSTICS_TEST_HELPER_H_
