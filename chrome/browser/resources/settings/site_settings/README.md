# Content Settings UI in Desktop

[TOC]

## Overview

Content Settings are settings pages that live under chrome://settings/content.
They are intended to provide the user with information about the status of
sites' capabilities as well as to allow them to tweak these settings as they see
fit.

Behind the scenes, these are simple "html" pages using the
[polymer](https://www.polymer-project.org/) JavaScript library.

All content settings pages live under this folder and under the
[../site_settings_page](https://cs.chromium.org/chromium/src/chrome/browser/resources/settings/site_settings_page/)
folder. Arguably, the most important pages are:

*   [site_settings_page.html](https://cs.chromium.org/chromium/src/chrome/browser/resources/settings/site_settings_page/site_settings_page.html?type=cs&g=0)
    is the main settings page (`chrome://settings/content`).
*   [all_sites.html](https://cs.chromium.org/chromium/src/chrome/browser/resources/settings/site_settings/all_sites.html)
    lists all sites that have any relevant information to the users
    (`chrome://settings/content/all`).
*   [site_details.html](https://cs.chromium.org/chromium/src/chrome/browser/resources/settings/site_settings/site_details.html?type=cs&g=0)
    displays a detailed page for a particular origin.

Here are some common patterns/coding practices/tips that you might find useful:

## Strings

Any and all strings that are displayed to the user need to go through
internationalization (`i18n`) functions to ensure the string is correctly
localized to the language settings of the user. Examples:

```
<site-details-permission
  category="{{ContentSettingsTypes.SITE_SETTINGS_SOUND}}"
  icon="settings:volume-up" id="siteSettingsSound"
  label="$i18n{siteSettingsSound}">
</site-details-permission>
```

```
<option value="[[sortMethods_.STORAGE]]">
  $i18n{siteSettingsAllSitesSortMethodStorage}
</option>
```

```
<cr-button class="action-button" on-click="onResetSettings_">
  $i18n{siteSettingsSiteResetAll}
</cr-button>
```

```
<div slot="body">
  [[i18n('siteSettingsSiteResetConfirmation', pageTitle)]]
</div>
```

The string ids are mapped in
[settings_localized_strings_provider.cc](https://cs.chromium.org/chromium/src/chrome/browser/ui/webui/settings/settings_localized_strings_provider.cc)
to `IDS_` chrome resource strings ids.

## Updating prefs

If a toggle simply controls a pref there is a built-in control that allows you
to do this easily: `settings-toggle-button`.

Examples:

```
<settings-toggle-button id="toggle" class="two-line"
  label="$i18n{siteSettingsPdfDownloadPdfs}"
  pref="{{prefs.plugins.always_open_pdf_externally}}">
</settings-toggle-button>
```

or with a dynamically selected pref:

```
<settings-toggle-button id="toggle"
  pref="{{controlParams_}}"
  label="[[optionLabel_]]" sub-label="[[optionDescription_]]"
  disabled$="[[isToggleDisabled_(category)]]">
</settings-toggle-button>
```

If you need something more complicated than a simple true/false pref you can
make use of the `this.browserProxy` object which allows you to communicate with
the browser.

The class is declared in
[site_settings_prefs_browser_proxy.js](https://cs.chromium.org/chromium/src/chrome/browser/resources/settings/site_settings/site_settings_prefs_browser_proxy.js)
and the browser implementation resides in
[site_settings_handler.h](https://cs.chromium.org/chromium/src/chrome/browser/ui/webui/settings/site_settings_handler.h?type=cs&g=0).
Make sure to
[register](https://cs.chromium.org/chromium/src/chrome/browser/ui/webui/settings/site_settings_handler.cc?type=cs&g=0&l=341)
your message so that everything is properly bound.

## Hide specific sections

Often a particular section of a page only needs to be displayed only under
specific circumstances. In this case make liberal use of the
[dom-if template](https://polymer-library.polymer-project.org/1.0/api/elements/dom-if).

Examples:

```
// html:

<template is="dom-if" if="[[enableInsecureContentContentSetting_]]">
  <site-details-permission category="{{ContentSettingsTypes.MIXEDSCRIPT}}"
    icon="settings:insecure-content" id="mixed-script"
    label="$i18n{siteSettingsInsecureContent}">
  </site-details-permission>
</template>
```

```
// js:

Polymer({

// ...

properties: {
  /** @private */ enableInsecureContentContentSetting_: {
    type: Boolean,
    value() {
      return loadTimeData.getBoolean('enableInsecureContentContentSetting');
    }
  },

  // ...
```

## Platform specific elements

You can limit the visibility of an element to a certain platform, by using the
available expressions.

Examples:

```
<if expr="chromeos_ash">
  <link rel="import" href="android_info_browser_proxy.html">
</if>
```

```
<if expr="chromeos_ash">
  <template is="dom-if" if="[[settingsAppAvailable_]]">
    <cr-link-row on-click="onManageAndroidAppsClick_"
        label="$i18n{androidAppsManageAppLinks}" external></cr-link-row>
  </template>
</if>
```
