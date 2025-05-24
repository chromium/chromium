// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/test/test_extension_dir.h"

namespace extensions {

class DelayedSettingChangeTest
    : public ExtensionBrowserTest,
      public testing::WithParamInterface<std::tuple<std::string, bool>> {
 protected:
  const Extension* InstallTestExtension(mojom::ManifestLocation location) {
    TestExtensionDir extension_dir;
    extension_dir.WriteFile(
        FILE_PATH_LITERAL("index.html"),
        "<html><body><div>Just a Chrome Extension</div></body></html>");

    extension_dir.WriteManifest(
        base::Value::Dict()
            .Set("manifest_version", 3)
            .Set("name", "Test Managed Extension")
            .Set("version", "1")
            .Set("host_permissions", base::Value::List().Append("file://*/*")));

    auto* extension = InstallExtensionWithSourceAndFlags(
        extension_dir.Pack(), 1, location,
        Extension::InitFromValueFlags::NO_FLAGS);
    CHECK(extension);

    return extension;
  }

  std::string GetScript_WarningMessageIsHidden(std::string_view setting) {
    // NOTE: This relies on the layout of the chrome://extensions page, and may
    // need to be updated if that layout changes.
    static constexpr std::string_view kScriptTemplate =
        R"((function() {
          return document.
            querySelector('extensions-manager').shadowRoot.
            querySelector('extensions-detail-view').shadowRoot.
            querySelector('%s').
            querySelector('%s-warning').hidden;
    })();)";

    return base::StringPrintf(kScriptTemplate, setting.data(), setting.data());
  }

  std::string GetScript_SwitchIsOn(std::string_view setting) {
    // NOTE: This relies on the layout of the chrome://extensions page, and may
    // need to be updated if that layout changes.
    static constexpr std::string_view kScriptTemplate =
        R"((function() {
          return document.
            querySelector('extensions-manager').shadowRoot.
            querySelector('extensions-detail-view').shadowRoot.
            querySelector('%s').shadowRoot.
            querySelector('cr-toggle').ariaPressed;
    })();)";

    return base::StringPrintf(kScriptTemplate, setting.data());
  }

  std::string GetScript_ClickSwitch(std::string_view setting) {
    // NOTE: This relies on the layout of the chrome://extensions page, and may
    // need to be updated if that layout changes.
    static constexpr std::string_view kScriptTemplate =
        R"((function() {
          document.
            querySelector('extensions-manager').shadowRoot.
            querySelector('extensions-detail-view').shadowRoot.
            querySelector('%s').shadowRoot.
            querySelector('cr-toggle').click();
    })();)";

    return base::StringPrintf(kScriptTemplate, setting.data());
  }
};

INSTANTIATE_TEST_SUITE_P(
    DelayedSetting,
    DelayedSettingChangeTest,
    ::testing::Combine(::testing::Values("#allow-incognito",
                                         "#allow-on-file-urls"),
                       ::testing::Bool()));

IN_PROC_BROWSER_TEST_P(DelayedSettingChangeTest,
                       DetailsPageOfManagedExtension) {
  scoped_refptr<const Extension> extension =
      InstallTestExtension(mojom::ManifestLocation::kExternalPolicy);
  ASSERT_TRUE(extension.get());

  auto [setting_under_test, initial_value] = GetParam();

  ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(profile());
  if (setting_under_test == "#allow-incognito") {
    extension_prefs->SetIsIncognitoEnabled(extension->id(), initial_value);
  } else {
    extension_prefs->SetAllowFileAccess(extension->id(), initial_value);
  }

  // Go to the extension details page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("chrome://extensions?id=" + extension->id())));

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(tab));

  // Initial state after installation.
  EXPECT_EQ(true, content::EvalJs(tab, GetScript_WarningMessageIsHidden(
                                           setting_under_test)));
  EXPECT_EQ(initial_value,
            content::EvalJs(tab, GetScript_SwitchIsOn(setting_under_test))
                    .ExtractString() == "true");

  // Change the value of the setting.
  EXPECT_EQ(true,
            content::ExecJs(tab, GetScript_ClickSwitch(setting_under_test)));

  // The warning message should now be visible.
  EXPECT_EQ(false, content::EvalJs(tab, GetScript_WarningMessageIsHidden(
                                            setting_under_test)));

  // The status of the switch should have changed.
  EXPECT_NE(initial_value,
            content::EvalJs(tab, GetScript_SwitchIsOn(setting_under_test))
                    .ExtractString() == "true");
}

IN_PROC_BROWSER_TEST_P(DelayedSettingChangeTest, DetailsPageOfOtherExtension) {
  scoped_refptr<const Extension> managed_extension =
      InstallTestExtension(mojom::ManifestLocation::kExternalPolicy);
  ASSERT_TRUE(managed_extension.get());

  scoped_refptr<const Extension> second_extension =
      InstallTestExtension(mojom::ManifestLocation::kExternalPolicy);
  ASSERT_TRUE(second_extension.get());
  ASSERT_NE(managed_extension->id(), second_extension->id());

  auto [setting_under_test, initial_value] = GetParam();

  ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(profile());
  if (setting_under_test == "#allow-incognito") {
    extension_prefs->SetIsIncognitoEnabled(managed_extension->id(),
                                           initial_value);
  } else {
    extension_prefs->SetAllowFileAccess(managed_extension->id(), initial_value);
  }

  // Go to the managed extension details page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("chrome://extensions?id=" + managed_extension->id())));
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(tab));

  // Initial state after installation.
  EXPECT_EQ(true, content::EvalJs(tab, GetScript_WarningMessageIsHidden(
                                           setting_under_test)));
  EXPECT_EQ(initial_value,
            content::EvalJs(tab, GetScript_SwitchIsOn(setting_under_test))
                    .ExtractString() == "true");

  // Change the value of the setting.
  EXPECT_EQ(true,
            content::ExecJs(tab, GetScript_ClickSwitch(setting_under_test)));

  // The warning message should now be visible.
  EXPECT_EQ(false, content::EvalJs(tab, GetScript_WarningMessageIsHidden(
                                            setting_under_test)));

  // Go to the 2nd extension details page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("chrome://extensions?id=" + second_extension->id())));
  tab = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(tab));

  EXPECT_EQ(true, content::EvalJs(tab, GetScript_WarningMessageIsHidden(
                                           setting_under_test)));

  // Return to the managed extension details page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("chrome://extensions?id=" + managed_extension->id())));
  tab = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(tab));

  // The warning message for the managed extension should be visible.
  EXPECT_EQ(false, content::EvalJs(tab, GetScript_WarningMessageIsHidden(
                                            setting_under_test)));

  // The status of the switch should have changed.
  EXPECT_NE(initial_value,
            content::EvalJs(tab, GetScript_SwitchIsOn(setting_under_test))
                    .ExtractString() == "true");
}
}  // namespace extensions
