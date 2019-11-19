// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Behavior common to Site Settings classes.
 */


/**
 * The source information on site exceptions doesn't exactly match the
 * controlledBy values.
 * TODO(dschuyler): Can they be unified (and this dictionary removed)?
 * @type {!Object}
 */
const kControlledByLookup = {
  'extension': chrome.settingsPrivate.ControlledBy.EXTENSION,
  'HostedApp': chrome.settingsPrivate.ControlledBy.EXTENSION,
  'platform_app': chrome.settingsPrivate.ControlledBy.EXTENSION,
  'policy': chrome.settingsPrivate.ControlledBy.USER_POLICY,
};


/** @polymerBehavior */
const SiteSettingsBehaviorImpl = {
  properties: {
    /**
     * The string ID of the category this element is displaying data for.
     * See site_settings/constants.js for possible values.
     * @type {!settings.ContentSettingsTypes}
     */
    category: String,

    /**
     * A cached list of ContentSettingsTypes with a standard allow-block-ask
     * pattern that are currently enabled for use. This property is the same
     * across all elements with SiteSettingsBehavior ('static').
     * @type {Array<settings.ContentSettingsTypes>}
     * @private
     */
    contentTypes_: {
      type: Array,
      value: [],
    },

    /**
     * The browser proxy used to retrieve and change information about site
     * settings categories and the sites within.
     * @type {settings.SiteSettingsPrefsBrowserProxy}
     */
    browserProxy: Object,
  },

  /** @override */
  created: function() {
    this.browserProxy =
        settings.SiteSettingsPrefsBrowserProxyImpl.getInstance();
  },

  /** @override */
  ready: function() {
    this.ContentSetting = settings.ContentSetting;
  },

  /**
   * Ensures the URL has a scheme (assumes http if omitted).
   * @param {string} url The URL with or without a scheme.
   * @return {string} The URL with a scheme, or an empty string.
   */
  ensureUrlHasScheme: function(url) {
    if (url.length == 0) {
      return url;
    }
    return url.includes('://') ? url : 'http://' + url;
  },

  /**
   * Removes redundant ports, such as port 80 for http and 443 for https.
   * @param {string} url The URL to sanitize.
   * @return {string} The URL without redundant ports, if any.
   */
  sanitizePort: function(url) {
    const urlWithScheme = this.ensureUrlHasScheme(url);
    if (urlWithScheme.startsWith('https://') &&
        urlWithScheme.endsWith(':443')) {
      return url.slice(0, -4);
    }
    if (urlWithScheme.startsWith('http://') && urlWithScheme.endsWith(':80')) {
      return url.slice(0, -3);
    }
    return url;
  },

  /**
   * Returns true if the passed content setting is considered 'enabled'.
   * @param {string} setting
   * @return {boolean}
   * @protected
   */
  computeIsSettingEnabled: function(setting) {
    return setting != settings.ContentSetting.BLOCK;
  },

  /**
   * Converts a string origin/pattern to a URL.
   * @param {string} originOrPattern The origin/pattern to convert to URL.
   * @return {URL} The URL to return (or null if origin is not a valid URL).
   * @protected
   */
  toUrl: function(originOrPattern) {
    if (originOrPattern.length == 0) {
      return null;
    }
    // TODO(finnur): Hmm, it would probably be better to ensure scheme on the
    //     JS/C++ boundary.
    // TODO(dschuyler): I agree. This filtering should be done in one go, rather
    // that during the sort. The URL generation should be wrapped in a try/catch
    // as well.
    originOrPattern = originOrPattern.replace('*://', '');
    originOrPattern = originOrPattern.replace('[*.]', '');
    return new URL(this.ensureUrlHasScheme(originOrPattern));
  },

  /**
   * Convert an exception (received from the C++ handler) to a full
   * SiteException.
   * @param {!RawSiteException} exception The raw site exception from C++.
   * @return {!SiteException} The expanded (full) SiteException.
   * @protected
   */
  expandSiteException: function(exception) {
    const origin = exception.origin;
    const embeddingOrigin = exception.embeddingOrigin;

    // TODO(patricialor): |exception.source| should be one of the values defined
    // in |settings.SiteSettingSource|.
    let enforcement = /** @type {?chrome.settingsPrivate.Enforcement} */ (null);
    if (exception.source == 'extension' || exception.source == 'HostedApp' ||
        exception.source == 'platform_app' || exception.source == 'policy') {
      enforcement = chrome.settingsPrivate.Enforcement.ENFORCED;
    }

    const controlledBy = /** @type {!chrome.settingsPrivate.ControlledBy} */ (
        kControlledByLookup[exception.source] ||
        chrome.settingsPrivate.ControlledBy.PRIMARY_USER);

    return {
      category: this.category,
      origin: origin,
      displayName: exception.displayName,
      embeddingOrigin: embeddingOrigin,
      incognito: exception.incognito,
      setting: exception.setting,
      enforcement: enforcement,
      controlledBy: controlledBy,
    };
  },

  /**
   * Returns list of categories for each setting.ContentSettingsTypes that are
   * currently enabled.
   * @return {!Array<!settings.ContentSettingsTypes>}
   */
  getCategoryList: function() {
    if (this.contentTypes_.length == 0) {
      for (const typeName in settings.ContentSettingsTypes) {
        const contentType = settings.ContentSettingsTypes[typeName];
        // <if expr="not chromeos">
        if (contentType == settings.ContentSettingsTypes.PROTECTED_CONTENT) {
          continue;
        }
        // </if>
        // Some categories store their data in a custom way.
        if (contentType == settings.ContentSettingsTypes.COOKIES ||
            contentType == settings.ContentSettingsTypes.PROTOCOL_HANDLERS ||
            contentType == settings.ContentSettingsTypes.ZOOM_LEVELS) {
          continue;
        }
        this.contentTypes_.push(contentType);
      }
    }

    const addOrRemoveSettingWithFlag = (type, flag) => {
      if (loadTimeData.getBoolean(flag)) {
        if (!this.contentTypes_.includes(type)) {
          this.contentTypes_.push(type);
        }
      } else {
        if (this.contentTypes_.includes(type)) {
          this.contentTypes_.splice(this.contentTypes_.indexOf(type), 1);
        }
      }
    };
    // These categories are gated behind flags.
    addOrRemoveSettingWithFlag(
        settings.ContentSettingsTypes.BLUETOOTH_SCANNING,
        'enableExperimentalWebPlatformFeatures');
    addOrRemoveSettingWithFlag(
        settings.ContentSettingsTypes.ADS,
        'enableSafeBrowsingSubresourceFilter');
    addOrRemoveSettingWithFlag(
        settings.ContentSettingsTypes.PAYMENT_HANDLER,
        'enablePaymentHandlerContentSetting');
    addOrRemoveSettingWithFlag(
        settings.ContentSettingsTypes.NATIVE_FILE_SYSTEM_WRITE,
        'enableNativeFileSystemWriteContentSetting');
    addOrRemoveSettingWithFlag(
        settings.ContentSettingsTypes.MIXEDSCRIPT,
        'enableInsecureContentContentSetting');
    return this.contentTypes_.slice(0);
  },

};

/** @polymerBehavior */
const SiteSettingsBehavior = [SiteSettingsBehaviorImpl];
