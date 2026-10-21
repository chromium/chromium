---
name: chrome/test-preference-registration-and-propagation
rubric: testing
severity: should_fix
---

**Rule.** When introducing new preferences, default configuration values, or
mapping logic (such as script-specific font preferences or localized string
defaults), include automated unit or browser tests. Verify that the preferences
are registered in PrefRegistry, retain expected default values (including
localized or platform-specific resources), and correctly propagate updates to
downstream consumers (such as WebPreferences, PrefChangeRegistrar observers, or
renderer settings).

**Why.** Registering preference keys and default values without automated test
coverage risks silent regressions, including unregistered preference crashes,
missing or empty localized defaults across platforms, and broken change
propagation to consuming services or renderers.

## Examples

Bad:

```cpp
// Registered new font preferences in prefs_tab_helper.cc and added GRD defaults,
// but did not add any automated tests verifying registration or WebPreferences propagation.
void PrefsTabHelper::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry,
    const std::string& locale) {
  RegisterFontFamilyPrefs(registry, locale);
}
```

Good:

```cpp
IN_PROC_BROWSER_TEST_F(PrefsTabHelperBrowserTest, DevanagariDefaultPrefs) {
  PrefService* prefs = chrome_test_utils::GetProfile(this)->GetPrefs();
  const PrefService::Preference* standard_pref =
      prefs->FindPreference(prefs::kWebKitStandardFontFamilyDevanagari);
  ASSERT_TRUE(standard_pref);
  EXPECT_TRUE(standard_pref->IsDefaultValue());
  EXPECT_FALSE(
      prefs->GetString(prefs::kWebKitStandardFontFamilyDevanagari).empty());
}

IN_PROC_BROWSER_TEST_F(PrefsTabHelperBrowserTest, DevanagariFontFamilies) {
  PrefService* prefs = chrome_test_utils::GetProfile(this)->GetPrefs();
  prefs->SetString(prefs::kWebKitStandardFontFamilyDevanagari, "CustomDevaStandard");
  content::WebContents* web_contents = chrome_test_utils::GetActiveWebContents(this);
  web_contents->NotifyPreferencesChanged();
  blink::web_pref::WebPreferences web_prefs = web_contents->GetOrCreateWebPreferences();
  EXPECT_EQ(u"CustomDevaStandard", web_prefs.standard_font_family_map["Deva"]);
}
```
