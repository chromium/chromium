// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the "Site Settings" section to
 * interact with the content settings prefs.
 */

/**
 * The handler will send a policy source that is similar, but not exactly the
 * same as a ControlledBy value. If the ContentSettingProvider is omitted it
 * should be treated as 'default'.
 * @enum {string}
 */
const ContentSettingProvider = {
  POLICY: 'policy',
  SUPERVISED_USER: 'supervised_user',
  EXTENSION: 'extension',
  INSTALLED_WEBAPP_PROVIDER: 'installed_webapp_provider',
  NOTIFICATION_ANDROID: 'notification_android',
  EPHEMERAL: 'ephemeral',
  PREFERENCE: 'preference',
  DEFAULT: 'default',
  TESTS: 'tests',
  TESTS_OTHER: 'tests_other'
};

/**
 * Stores information about if a content setting is valid, and why.
 * @typedef {{isValid: boolean,
 *            reason: ?string}}
 */
let IsValid;

/**
 * Stores origin information. The |hasPermissionSettings| will be set to true
 * when this origin has permissions or when there is a pattern permission
 * affecting this origin.
 * @typedef {{origin: string,
 *            engagement: number,
 *            usage: number,
              numCookies: number,
              hasPermissionSettings: boolean,
              isInstalled: boolean}}
 */
let OriginInfo;

/**
 * Represents a list of sites, grouped under the same eTLD+1. For example, an
 * origin "https://www.example.com" would be grouped together with
 * "https://login.example.com" and "http://example.com" under a common eTLD+1 of
 * "example.com".
 * @typedef {{etldPlus1: string,
 *            numCookies: number,
 *            origins: Array<OriginInfo>,
 *            hasInstalledPWA: boolean}}
 */
let SiteGroup;

/**
 * The site exception information passed from the C++ handler.
 * See also: SiteException.
 * @typedef {{embeddingOrigin: string,
 *            incognito: boolean,
 *            origin: string,
 *            displayName: string,
 *            setting: !settings.ContentSetting,
 *            source: !settings.SiteSettingSource}}
 */
let RawSiteException;

/**
 * The site exception after it has been converted/filtered for UI use.
 * See also: RawSiteException.
 * @typedef {{category: !settings.ContentSettingsTypes,
 *            embeddingOrigin: string,
 *            incognito: boolean,
 *            origin: string,
 *            displayName: string,
 *            setting: !settings.ContentSetting,
 *            enforcement: ?chrome.settingsPrivate.Enforcement,
 *            controlledBy: !chrome.settingsPrivate.ControlledBy,
 *            showAndroidSmsNote: (boolean|undefined)}}
 */
let SiteException;

/**
 * The chooser exception information passed from the C++ handler.
 * See also: ChooserException.
 * @typedef {{chooserType: !settings.ChooserType,
 *            displayName: string,
 *            object: Object,
 *            sites: Array<!RawSiteException>}}
 */
let RawChooserException;

/**
 * The chooser exception after it has been converted/filtered for UI use.
 * See also: RawChooserException.
 * @typedef {{chooserType: !settings.ChooserType,
 *            displayName: string,
 *            object: Object,
 *            sites: Array<!SiteException>}}
 */
let ChooserException;

/**
 * @typedef {{setting: !settings.ContentSetting,
 *            source: !ContentSettingProvider}}
 */
let DefaultContentSetting;

/**
 * @typedef {{name: string,
 *            id: string}}
 */
let MediaPickerEntry;

/**
 * @typedef {{protocol: string,
 *            spec: string}}
 */
let ProtocolHandlerEntry;

/**
 * @typedef {{origin: string,
 *            setting: string,
 *            source: string,
 *            zoom: string}}
 */
let ZoomLevelEntry;

cr.define('settings', function() {
  /** @interface */
  class SiteSettingsPrefsBrowserProxy {
    /**
     * Sets the default value for a site settings category.
     * @param {string} contentType The name of the category to change.
     * @param {string} defaultValue The name of the value to set as default.
     */
    setDefaultValueForContentType(contentType, defaultValue) {}

    /**
     * Gets the default value for a site settings category.
     * @param {string} contentType The name of the category to query.
     * @return {!Promise<!DefaultContentSetting>}
     */
    getDefaultValueForContentType(contentType) {}

    /**
     * Gets a list of sites, grouped by eTLD+1, affected by any of the content
     * settings specified by |contentTypes|.
     * @param {!Array<!settings.ContentSettingsTypes>} contentTypes A list of
     *     the content types to retrieve sites for.
     * @return {!Promise<!Array<!SiteGroup>>}
     */
    getAllSites(contentTypes) {}

    /**
     * Gets the chooser exceptions for a particular chooser type.
     * @param {settings.ChooserType} chooserType The chooser type to grab
     *     exceptions from.
     * @return {!Promise<!Array<!RawChooserException>>}
     */
    getChooserExceptionList(chooserType) {}

    /**
     * Converts a given number of bytes into a human-readable format, with data
     * units.
     * @param {number} numBytes The number of bytes to convert.
     * @return {!Promise<string>}
     */
    getFormattedBytes(numBytes) {}

    /**
     * Gets the exceptions (site list) for a particular category.
     * @param {string} contentType The name of the category to query.
     * @return {!Promise<!Array<!RawSiteException>>}
     */
    getExceptionList(contentType) {}

    /**
     * Gets a list of category permissions for a given origin. Note that this
     * may be different to the results retrieved by getExceptionList(), since it
     * combines different sources of data to get a permission's value.
     * @param {string} origin The origin to look up permissions for.
     * @param {!Array<!settings.ContentSettingsTypes>} contentTypes A list of
     *     categories to retrieve the ContentSetting for.
     * @return {!Promise<!NodeList<!RawSiteException>>}
     */
    getOriginPermissions(origin, contentTypes) {}

    /**
     * Resets the permissions for a list of categories for a given origin. This
     * does not support incognito settings or patterns.
     * @param {string} origin The origin to reset permissions for.
     * @param {!Array<!settings.ContentSettingsTypes>} contentTypes A list of
     *     categories to set the permission for. Typically this would be a
     *     single category, but sometimes it is useful to clear any permissions
     *     set for all categories.
     * @param {!settings.ContentSetting} blanketSetting The setting to set all
     *     permissions listed in |contentTypes| to.
     */
    setOriginPermissions(origin, contentTypes, blanketSetting) {}

    /**
     * Clears the flag that's set when the user has changed the Flash permission
     * for this particular origin.
     * @param {string} origin The origin to clear the Flash preference for.
     */
    clearFlashPref(origin) {}

    /**
     * Resets the category permission for a given origin (expressed as primary
     * and secondary patterns). Only use this if intending to remove an
     * exception - use setOriginPermissions() for origin-scoped settings.
     * @param {string} primaryPattern The origin to change (primary pattern).
     * @param {string} secondaryPattern The embedding origin to change
     *     (secondary pattern).
     * @param {string} contentType The name of the category to reset.
     * @param {boolean} incognito Whether this applies only to a current
     *     incognito session exception.
     */
    resetCategoryPermissionForPattern(
        primaryPattern, secondaryPattern, contentType, incognito) {}

    /**
     * Removes a particular chooser object permission by origin and embedding
     * origin.
     * @param {settings.ChooserType} chooserType The chooser exception type
     * @param {string} origin The origin to look up the permission for.
     * @param {string} embeddingOrigin the embedding origin to look up.
     * @param {!Object} exception The exception to revoke permission for.
     */
    resetChooserExceptionForSite(
        chooserType, origin, embeddingOrigin, exception) {}

    /**
     * Sets the category permission for a given origin (expressed as primary and
     * secondary patterns). Only use this if intending to set an exception - use
     * setOriginPermissions() for origin-scoped settings.
     * @param {string} primaryPattern The origin to change (primary pattern).
     * @param {string} secondaryPattern The embedding origin to change
     *     (secondary pattern).
     * @param {string} contentType The name of the category to change.
     * @param {string} value The value to change the permission to.
     * @param {boolean} incognito Whether this rule applies only to the current
     *     incognito session.
     */
    setCategoryPermissionForPattern(
        primaryPattern, secondaryPattern, contentType, value, incognito) {}

    /**
     * Checks whether an origin is valid.
     * @param {string} origin The origin to check.
     * @return {!Promise<boolean>} True if the origin is valid.
     */
    isOriginValid(origin) {}

    /**
     * Checks whether a setting is valid.
     * @param {string} pattern The pattern to check.
     * @param {settings.ContentSettingsTypes} category What kind of setting,
     *     e.g. Location, Camera, Cookies, etc.
     * @return {!Promise<IsValid>} Contains whether or not the pattern is
     *     valid for the type, and if it is invalid, the reason why.
     */
    isPatternValidForType(pattern, category) {}

    /**
     * Gets the list of default capture devices for a given type of media. List
     * is returned through a JS call to updateDevicesMenu.
     * @param {string} type The type to look up.
     */
    getDefaultCaptureDevices(type) {}

    /**
     * Sets a default devices for a given type of media.
     * @param {string} type The type of media to configure.
     * @param {string} defaultValue The id of the media device to set.
     */
    setDefaultCaptureDevice(type, defaultValue) {}

    /**
     * observes _all_ of the the protocol handler state, which includes a list
     * that is returned through JS calls to 'setProtocolHandlers' along with
     * other state sent with the messages 'setIgnoredProtocolHandler' and
     * 'setHandlersEnabled'.
     */
    observeProtocolHandlers() {}

    /**
     * Observes one aspect of the protocol handler so that updates to the
     * enabled/disabled state are sent. A 'setHandlersEnabled' will be sent
     * from C++ immediately after receiving this observe request and updates
     * may follow via additional 'setHandlersEnabled' messages.
     *
     * If |observeProtocolHandlers| is called, there's no need to call this
     * observe as well.
     */
    observeProtocolHandlersEnabledState() {}

    /**
     * Enables or disables the ability for sites to ask to become the default
     * protocol handlers.
     * @param {boolean} enabled Whether sites can ask to become default.
     */
    setProtocolHandlerDefault(enabled) {}

    /**
     * Sets a certain url as default for a given protocol handler.
     * @param {string} protocol The protocol to set a default for.
     * @param {string} url The url to use as the default.
     */
    setProtocolDefault(protocol, url) {}

    /**
     * Deletes a certain protocol handler by url.
     * @param {string} protocol The protocol to delete the url from.
     * @param {string} url The url to delete.
     */
    removeProtocolHandler(protocol, url) {}

    /**
     * Fetches the incognito status of the current profile (whether an incognito
     * profile exists). Returns the results via onIncognitoStatusChanged.
     */
    updateIncognitoStatus() {}

    /**
     * Fetches the currently defined zoom levels for sites. Returns the results
     * via onZoomLevelsChanged.
     */
    fetchZoomLevels() {}

    /**
     * Removes a zoom levels for a given host.
     * @param {string} host The host to remove zoom levels for.
     */
    removeZoomLevel(host) {}

    // <if expr="chromeos">
    /**
     * Links to com.android.settings.Settings$ManageDomainUrlsActivity on ARC
     * side, this is to manage app preferences.
     */
    showAndroidManageAppLinks() {}
    // </if>

    /**
     * Fetches the current block autoplay state. Returns the results via
     * onBlockAutoplayStatusChanged.
     */
    fetchBlockAutoplayStatus() {}

    /**
     * Clears all the web storage data and cookies for a given etld+1.
     * @param {string} etldPlus1 The etld+1 to clear data from.
     */
    clearEtldPlus1DataAndCookies(etldPlus1) {}

    /**
     * Record All Sites Page action for metrics.
     *  @param {number} action number.
     */
    recordAction(action) {}
  }

  /**
   * @implements {settings.SiteSettingsPrefsBrowserProxy}
   */
  class SiteSettingsPrefsBrowserProxyImpl {
    /** @override */
    setDefaultValueForContentType(contentType, defaultValue) {
      chrome.send('setDefaultValueForContentType', [contentType, defaultValue]);
    }

    /** @override */
    getDefaultValueForContentType(contentType) {
      return cr.sendWithPromise('getDefaultValueForContentType', contentType);
    }

    /** @override */
    getAllSites(contentTypes) {
      return cr.sendWithPromise('getAllSites', contentTypes);
    }

    /** @override */
    getChooserExceptionList(chooserType) {
      return cr.sendWithPromise('getChooserExceptionList', chooserType);
    }

    /** @override */
    getFormattedBytes(numBytes) {
      return cr.sendWithPromise('getFormattedBytes', numBytes);
    }

    /** @override */
    getExceptionList(contentType) {
      return cr.sendWithPromise('getExceptionList', contentType);
    }

    /** @override */
    getOriginPermissions(origin, contentTypes) {
      return cr.sendWithPromise('getOriginPermissions', origin, contentTypes);
    }

    /** @override */
    setOriginPermissions(origin, contentTypes, blanketSetting) {
      chrome.send(
          'setOriginPermissions', [origin, contentTypes, blanketSetting]);
    }

    /** @override */
    clearFlashPref(origin) {
      chrome.send('clearFlashPref', [origin]);
    }

    /** @override */
    resetCategoryPermissionForPattern(
        primaryPattern, secondaryPattern, contentType, incognito) {
      chrome.send(
          'resetCategoryPermissionForPattern',
          [primaryPattern, secondaryPattern, contentType, incognito]);
    }

    /** @override */
    resetChooserExceptionForSite(
        chooserType, origin, embeddingOrigin, exception) {
      chrome.send(
          'resetChooserExceptionForSite',
          [chooserType, origin, embeddingOrigin, exception]);
    }

    /** @override */
    setCategoryPermissionForPattern(
        primaryPattern, secondaryPattern, contentType, value, incognito) {
      chrome.send(
          'setCategoryPermissionForPattern',
          [primaryPattern, secondaryPattern, contentType, value, incognito]);
    }

    /** @override */
    isOriginValid(origin) {
      return cr.sendWithPromise('isOriginValid', origin);
    }

    /** @override */
    isPatternValidForType(pattern, category) {
      return cr.sendWithPromise('isPatternValidForType', pattern, category);
    }

    /** @override */
    getDefaultCaptureDevices(type) {
      chrome.send('getDefaultCaptureDevices', [type]);
    }

    /** @override */
    setDefaultCaptureDevice(type, defaultValue) {
      chrome.send('setDefaultCaptureDevice', [type, defaultValue]);
    }

    /** @override */
    observeProtocolHandlers() {
      chrome.send('observeProtocolHandlers');
    }

    /** @override */
    observeProtocolHandlersEnabledState() {
      chrome.send('observeProtocolHandlersEnabledState');
    }

    /** @override */
    setProtocolHandlerDefault(enabled) {
      chrome.send('setHandlersEnabled', [enabled]);
    }

    /** @override */
    setProtocolDefault(protocol, url) {
      chrome.send('setDefault', [protocol, url]);
    }

    /** @override */
    removeProtocolHandler(protocol, url) {
      chrome.send('removeHandler', [protocol, url]);
    }

    /** @override */
    updateIncognitoStatus() {
      chrome.send('updateIncognitoStatus');
    }

    /** @override */
    fetchZoomLevels() {
      chrome.send('fetchZoomLevels');
    }

    /** @override */
    removeZoomLevel(host) {
      chrome.send('removeZoomLevel', [host]);
    }

    // <if expr="chromeos">
    /** @override */
    showAndroidManageAppLinks() {
      chrome.send('showAndroidManageAppLinks');
    }
    // </if>

    /** @override */
    fetchBlockAutoplayStatus() {
      chrome.send('fetchBlockAutoplayStatus');
    }

    /** @override */
    clearEtldPlus1DataAndCookies(etldPlus1) {
      chrome.send('clearEtldPlus1DataAndCookies', [etldPlus1]);
    }

    /** @override */
    recordAction(action) {
      chrome.send('recordAction', [action]);
    }
  }

  // The singleton instance_ is replaced with a test version of this wrapper
  // during testing.
  cr.addSingletonGetter(SiteSettingsPrefsBrowserProxyImpl);

  return {
    SiteSettingsPrefsBrowserProxy: SiteSettingsPrefsBrowserProxy,
    SiteSettingsPrefsBrowserProxyImpl: SiteSettingsPrefsBrowserProxyImpl,
  };
});
