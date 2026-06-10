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

IN_PROC_BROWSER_TEST_F(TerminalPrivateBrowserTest,
                       OnProcessOutputSendToCorrectRenderer) {
  // Open 2 tabs in crosh.  Crosh will echo input for tests.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL("chrome-untrusted://crosh/")));
  content::WebContents* tab1 =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("chrome-untrusted://crosh/"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  content::WebContents* tab2 =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Verify that we have two distinct tabs.
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  ASSERT_NE(tab1, tab2);

  // In the second tab, register a listener for the onProcessOutput event.
  // This listener will increment a counter every time it receives an event.
  const std::string script2 = R"(
    window.processOutputCount = 0;
    chrome.terminalPrivate.onProcessOutput.addListener((id, type, data) => {
      window.processOutputCount++;
    });
    true;
  )";
  EXPECT_EQ(true, EvalJs(tab2, script2, content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                         /*world_id=*/1));

  // In the first tab, send input and verify it gets echoed back.
  const std::string script1 = R"(new Promise((resolve) => {
    chrome.terminalPrivate.onProcessOutput.addListener((id, type, data) => {
      resolve(new TextDecoder().decode(data));
    });
    chrome.terminalPrivate.openTerminalProcess("crosh", [], (id) => {
      if (chrome.runtime.lastError) {
        resolve(chrome.runtime.lastError.message);
        return;
      }
      chrome.terminalPrivate.sendInput(id, "hello", (success) => {});
    });
  }))";
  EXPECT_EQ("hello",
            EvalJs(tab1, script1, content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                   /*world_id=*/1));

  // Verify that the second tab did NOT receive the output event.
  EXPECT_EQ(0, EvalJs(tab2, "window.processOutputCount",
                      content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, /*world_id=*/1));
}

}  // namespace extensions
