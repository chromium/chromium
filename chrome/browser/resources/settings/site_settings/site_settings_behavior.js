// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Behavior common to Site Settings classes.
 */

// clang-format off
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

import {ContentSetting,ContentSettingsTypes} from './constants.js';
import {RawSiteException,SiteException,SiteSettingsPrefsBrowserProxy,SiteSettingsPrefsBrowserProxyImpl} from './site_settings_prefs_browser_proxy.js';
// clang-format on

/**
 * The source information on site exceptions doesn't exactly match the
 * controlledBy values.
 * TODO(dschuyler): Can they be unified (and this dictionary removed)?
 * @type {!Object}
 */
export const kControlledByLookup = {
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
     * @type {!ContentSettingsTypes}
     */
    category: String,

    /**
     * A cached list of ContentSettingsTypes with a standard allow-block-ask
     * pattern that are currently enabled for use. This property is the same
     * across all elements with SiteSettingsBehavior ('static').
     * @type {Array<ContentSettingsTypes>}
     * @private
     */
    contentTypes_: {
      type: Array,
      value: [],
    },

    /**
     * The browser proxy used to retrieve and change information about site
     * settings categories and the sites within.
     * @type {SiteSettingsPrefsBrowserProxy}
     */
    browserProxy: Object,
  },

  /** @override */
  created() {
    this.browserProxy = SiteSettingsPrefsBrowserProxyImpl.getInstance();
  },

  /** @override */
  ready() {
    this.ContentSetting = ContentSetting;
  },

  /**
   * Ensures the URL has a scheme (assumes http if omitted).
   * @param {string} url The URL with or without a scheme.
   * @return {string} The URL with a scheme, or an empty string.
   */
  ensureUrlHasScheme(url) {
    if (url.length === 0) {
      return url;
    }
    return url.includes('://') ? url : 'http://' + url;
  },

  /**
   * Removes redundant ports, such as port 80 for http and 443 for https.
   * @param {string} url The URL to sanitize.
   * @return {string} The URL without redundant ports, if any.
   */
  sanitizePort(url) {
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
  computeIsSettingEnabled(setting) {
    return setting !== ContentSetting.BLOCK;
  },

  /**
   * Converts a string origin/pattern to a URL.
   * @param {string} originOrPattern The origin/pattern to convert to URL.
   * @return {URL} The URL to return (or null if origin is not a valid URL).
   * @protected
   */
  toUrl(originOrPattern) {
    if (originOrPattern.length === 0) {
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
   * Returns a user-friendly name for the origin.
   * @param {string} origin
   * @return {string} The user-friendly name.
   * @protected
   */
  originRepresentation(origin) {
    try {
      const url = this.toUrl(origin);
      return url ? (url.host || url.origin) : '';
    } catch (error) {
      return '';
    }
  },

  /**
   * Convert an exception (received from the C++ handler) to a full
   * SiteException.
   * @param {!RawSiteException} exception The raw site exception from C++.
   * @return {!SiteException} The expanded (full) SiteException.
   * @protected
   */
  expandSiteException(exception) {
    const origin = exception.origin;
    const embeddingOrigin = exception.embeddingOrigin;

    // TODO(patricialor): |exception.source| should be one of the values defined
    // in |SiteSettingSource|.
    let enforcement = /** @type {?chrome.settingsPrivate.Enforcement} */ (null);
    if (exception.source === 'extension' || exception.source === 'HostedApp' ||
        exception.source === 'platform_app' || exception.source === 'policy') {
      enforcement = chrome.settingsPrivate.Enforcement.ENFORCED;
    }

    const controlledBy = /** @type {!chrome.settingsPrivate.ControlledBy} */ (
        kControlledByLookup[exception.source] ||
        chrome.settingsPrivate.ControlledBy.PRIMARY_USER);

    return {
      category: this.category,
      embeddingOrigin: embeddingOrigin,
      incognito: exception.incognito,
      isEmbargoed: exception.isEmbargoed,
      isDiscarded: exception.isDiscarded,
      origin: origin,
      displayName: exception.displayName,
      setting: exception.setting,
      enforcement: enforcement,
      controlledBy: controlledBy,
    };
  },

  /**
   * Returns list of categories for each setting.ContentSettingsTypes that are
   * currently enabled.
   * @return {!Array<!ContentSettingsTypes>}
   */
  getCategoryList() {
    if (this.contentTypes_.length === 0) {
      for (const typeName in ContentSettingsTypes) {
        const contentType = ContentSettingsTypes[typeName];
        // <if expr="not chromeos">
        if (contentType === ContentSettingsTypes.PROTECTED_CONTENT) {
          continue;
        }
        // </if>
        // Some categories store their data in a custom way.
        if (contentType === ContentSettingsTypes.COOKIES ||
            contentType === ContentSettingsTypes.PROTOCOL_HANDLERS ||
            contentType === ContentSettingsTypes.ZOOM_LEVELS) {
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
        ContentSettingsTypes.BLUETOOTH_SCANNING,
        'enableExperimentalWebPlatformFeatures');
    addOrRemoveSettingWithFlag(
        ContentSettingsTypes.ADS, 'enableSafeBrowsingSubresourceFilter');
    addOrRemoveSettingWithFlag(
        ContentSettingsTypes.PAYMENT_HANDLER,
        'enablePaymentHandlerContentSetting');
    addOrRemoveSettingWithFlag(
        ContentSettingsTypes.BLUETOOTH_DEVICES,
        'enableWebBluetoothNewPermissionsBackend');
    addOrRemoveSettingWithFlag(
        ContentSettingsTypes.WINDOW_PLACEMENT,
        'enableExperimentalWebPlatformFeatures');
    addOrRemoveSettingWithFlag(
        ContentSettingsTypes.FONT_ACCESS, 'enableFontAccessContentSetting');
    return this.contentTypes_.slice(0);
  },

};

/** @polymerBehavior */
export const SiteSettingsBehavior = [SiteSettingsBehaviorImpl];
