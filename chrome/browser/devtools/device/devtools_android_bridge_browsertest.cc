// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <algorithm>
#include <array>

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/devtools/device/devtools_android_bridge.h"
#include "chrome/browser/devtools/device/tcp_device_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"

class DevToolsAndroidBridgeTest : public InProcessBrowserTest {
};

static void assign_from_callback(scoped_refptr<TCPDeviceProvider>* store,
                                 int* invocation_counter,
                                 scoped_refptr<TCPDeviceProvider> value) {
  (*invocation_counter)++;
  *store = value;
}

static std::string SetToString(const std::set<std::string>& values) {
  std::ostringstream result;
  std::copy(values.begin(), values.end(),
            std::ostream_iterator<std::string>(result, ", "));
  std::string result_string = result.str();
  return result_string.substr(0, result_string.length() - 2);
}

static std::string AllTargetsString(
    scoped_refptr<TCPDeviceProvider> provider) {
  std::set<std::string> actual;
  for (const net::HostPortPair& hostport : provider->get_targets_for_test())
    actual.insert(hostport.ToString());
  return SetToString(actual);
}

IN_PROC_BROWSER_TEST_F(DevToolsAndroidBridgeTest, DiscoveryListChanges) {
  Profile* profile = browser()->profile();

  PrefService* service = profile->GetPrefs();
  service->ClearPref(prefs::kDevToolsTCPDiscoveryConfig);
  service->SetBoolean(prefs::kDevToolsDiscoverTCPTargetsEnabled, true);

  DevToolsAndroidBridge* bridge =
      DevToolsAndroidBridge::Factory::GetForProfile(profile);

  scoped_refptr<TCPDeviceProvider> provider;
  int called = 0;
  bridge->set_tcp_provider_callback_for_test(
      base::BindRepeating(assign_from_callback, &provider, &called));

  EXPECT_LT(0, called);
  EXPECT_NE(nullptr, provider);

  EXPECT_STREQ("localhost:9222, localhost:9229",
               AllTargetsString(provider).c_str());

  int invocations = called;
  base::ListValue list;
  list.AppendString("somehost:2000");

  service->Set(prefs::kDevToolsTCPDiscoveryConfig, list);

  EXPECT_LT(invocations, called);
  EXPECT_NE(nullptr, provider);
  EXPECT_STREQ("somehost:2000", AllTargetsString(provider).c_str());

  invocations = called;
  list.Clear();
  service->Set(prefs::kDevToolsTCPDiscoveryConfig, list);

  EXPECT_LT(invocations, called);
  EXPECT_EQ(nullptr, provider);
  invocations = called;

  list.AppendString("b:1");
  list.AppendString("c:2");
  list.AppendString("<not really a good address.");
  list.AppendString("d:3");
  list.AppendString("c:2");
  service->Set(prefs::kDevToolsTCPDiscoveryConfig, list);

  EXPECT_LT(invocations, called);
  EXPECT_NE(nullptr, provider);
  EXPECT_STREQ("b:1, c:2, d:3", AllTargetsString(provider).c_str());
}

IN_PROC_BROWSER_TEST_F(DevToolsAndroidBridgeTest, DefaultValues) {
  Profile* profile = browser()->profile();

  PrefService* service = profile->GetPrefs();
  DevToolsAndroidBridge::Factory::GetForProfile(profile);
  service->ClearPref(prefs::kDevToolsDiscoverTCPTargetsEnabled);
  service->ClearPref(prefs::kDevToolsTCPDiscoveryConfig);

  const base::ListValue* targets =
    service->GetList(prefs::kDevToolsTCPDiscoveryConfig);
  EXPECT_NE(nullptr, targets);
  EXPECT_EQ(2ul, targets->GetSize());

  std::set<std::string> actual;
  for (size_t i = 0; i < targets->GetSize(); i++) {
    std::string value;
    targets->GetString(i, &value);
    actual.insert(value);
  }
  EXPECT_STREQ("localhost:9222, localhost:9229", SetToString(actual).c_str());
  EXPECT_TRUE(service->GetBoolean(prefs::kDevToolsDiscoverTCPTargetsEnabled));
}

IN_PROC_BROWSER_TEST_F(DevToolsAndroidBridgeTest, TCPEnableChange) {
  Profile* profile = browser()->profile();

  PrefService* service = profile->GetPrefs();
  service->ClearPref(prefs::kDevToolsTCPDiscoveryConfig);
  service->ClearPref(prefs::kDevToolsDiscoverTCPTargetsEnabled);

  DevToolsAndroidBridge* bridge =
      DevToolsAndroidBridge::Factory::GetForProfile(profile);

  scoped_refptr<TCPDeviceProvider> provider;
  int called = 0;
  bridge->set_tcp_provider_callback_for_test(
      base::BindRepeating(assign_from_callback, &provider, &called));

  EXPECT_NE(nullptr, provider);

  service->SetBoolean(prefs::kDevToolsDiscoverTCPTargetsEnabled, true);

  EXPECT_NE(nullptr, provider);
  EXPECT_STREQ("localhost:9222, localhost:9229",
               AllTargetsString(provider).c_str());

  service->SetBoolean(prefs::kDevToolsDiscoverTCPTargetsEnabled, false);

  EXPECT_EQ(nullptr, provider);
}
