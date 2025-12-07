// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chrome/browser/ash/crostini/crostini_browser_test_util.h"
#include "chrome/browser/ash/crostini/fake_crostini_features.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/system_features_disable_list_policy_handler.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/policy/device_policy/cached_device_policy_updater.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/settings/cros_settings_waiter.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace extensions {

class TerminalPrivateBrowserTest
    : public InProcessBrowserTestMixinHostSupport<CrostiniBrowserTestBase> {
 public:
  TerminalPrivateBrowserTest(const TerminalPrivateBrowserTest&) = delete;
  TerminalPrivateBrowserTest& operator=(const TerminalPrivateBrowserTest&) =
      delete;

 protected:
  TerminalPrivateBrowserTest()
      : InProcessBrowserTestMixinHostSupport<CrostiniBrowserTestBase>(
            /*register_termina=*/false) {}

  void ExpectJsResult(const std::string& script, const std::string& expected) {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_EQ(
        EvalJs(web_contents, script, content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
               /*world_id=*/1),
        expected);
  }

  ash::DeviceStateMixin device_state_{
      &mixin_host_,
      ash::DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED};
};

IN_PROC_BROWSER_TEST_F(TerminalPrivateBrowserTest, OpenTerminalProcessChecks) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("chrome-untrusted://terminal/html/terminal.html")));

  const std::string script = R"(new Promise((resolve) => {
    chrome.terminalPrivate.openVmshellProcess([], () => {
      const lastError = chrome.runtime.lastError;
      resolve(lastError ? lastError.message : "success");
    })}))";

  // 'success' when VMs are allowed.
  {
    ash::CrosSettingsWaiter waiter({ash::kVirtualMachinesAllowed});
    policy::CachedDevicePolicyUpdater updater;
    updater.payload()
        .mutable_virtual_machines_allowed()
        ->set_virtual_machines_allowed(true);
    updater.Commit();
    waiter.Wait();
  }
  ExpectJsResult(script, "success");

  // 'vmshell not allowed' when VMs are not allowed.
  {
    ash::CrosSettingsWaiter waiter({ash::kVirtualMachinesAllowed});
    policy::CachedDevicePolicyUpdater updater;
    updater.payload()
        .mutable_virtual_machines_allowed()
        ->set_virtual_machines_allowed(false);
    updater.Commit();
    waiter.Wait();
  }
  ExpectJsResult(script, "vmshell not allowed");

  // openTerminalProcess not defined.
  ExpectJsResult("typeof chrome.terminalPrivate.openTerminalProcess",
                 "undefined");
}

IN_PROC_BROWSER_TEST_F(TerminalPrivateBrowserTest, OpenCroshProcessChecks) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("chrome-untrusted://crosh/html/crosh.html")));

  const std::string script = R"(new Promise((resolve) => {
    chrome.terminalPrivate.openTerminalProcess("crosh", [], () => {
      const lastError = chrome.runtime.lastError;
      resolve(lastError ? lastError.message : "success");
    })
    }))";

  base::Value::List system_features;
  system_features.Append(static_cast<int>(policy::SystemFeature::kCrosh));
  g_browser_process->local_state()->SetList(
      policy::policy_prefs::kSystemFeaturesDisableList,
      std::move(system_features));
  // 'crosh not allowed' when crosh is not allowed.
  ExpectJsResult(script, "crosh not allowed");

  g_browser_process->local_state()->SetList(
      policy::policy_prefs::kSystemFeaturesDisableList, base::Value::List());
  ExpectJsResult(script, "success");
}

}  // namespace extensions
