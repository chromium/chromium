// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/test/task_environment.h"
#include "chrome/browser/prefs/chrome_command_line_pref_store.h"
#include "chrome/browser/ssl/ssl_config_service_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_store.h"
#include "components/sync_preferences/pref_service_mock_factory.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/ssl_config.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

class CommandLinePrefStoreSSLManagerTest : public testing::Test {
 public:
  CommandLinePrefStoreSSLManagerTest() {}

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

// Test that command-line settings for SSL versions are respected and that they
// do not persist to the preferences files.
TEST_F(CommandLinePrefStoreSSLManagerTest, CommandLinePrefs) {
  scoped_refptr<TestingPrefStore> local_state_store(new TestingPrefStore());

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kSSLVersionMin, "tls1.1");
  command_line.AppendSwitchASCII(switches::kSSLVersionMax, "tls1.2");

  sync_preferences::PrefServiceMockFactory factory;
  factory.set_user_prefs(local_state_store);
  factory.set_command_line_prefs(new ChromeCommandLinePrefStore(&command_line));
  scoped_refptr<PrefRegistrySimple> registry = new PrefRegistrySimple;
  std::unique_ptr<PrefService> local_state(factory.Create(registry.get()));

  SSLConfigServiceManager::RegisterPrefs(registry.get());
  network::mojom::NetworkContextParamsPtr context_params =
      network::mojom::NetworkContextParams::New();
  std::unique_ptr<SSLConfigServiceManager> config_manager(
      SSLConfigServiceManager::CreateDefaultManager(local_state.get()));
  config_manager->AddToNetworkContextParams(context_params.get());

  // Command-line flags should be respected.
  EXPECT_EQ(network::mojom::SSLVersion::kTLS11,
            context_params->initial_ssl_config->version_min);
  EXPECT_EQ(network::mojom::SSLVersion::kTLS12,
            context_params->initial_ssl_config->version_max);

  // Explicitly double-check the settings are not in the preference store.
  const PrefService::Preference* version_min_pref =
      local_state->FindPreference(prefs::kSSLVersionMin);
  EXPECT_FALSE(version_min_pref->IsUserModifiable());

  const PrefService::Preference* version_max_pref =
      local_state->FindPreference(prefs::kSSLVersionMax);
  EXPECT_FALSE(version_max_pref->IsUserModifiable());

  std::string version_min_str;
  std::string version_max_str;
  EXPECT_FALSE(
      local_state_store->GetString(prefs::kSSLVersionMin, &version_min_str));
  EXPECT_FALSE(
      local_state_store->GetString(prefs::kSSLVersionMax, &version_max_str));
}
