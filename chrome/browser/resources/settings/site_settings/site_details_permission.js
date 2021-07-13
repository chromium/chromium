// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'site-details-permission' handles showing the state of one permission, such
 * as Geolocation, for a given origin.
 */
import 'chrome://resources/cr_elements/md_select_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../settings_shared_css.js';
import '../settings_vars_css.js';

import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.m.js';
import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';

import {ContentSetting, ContentSettingsTypes, SiteSettingSource} from './constants.js';
import {SiteSettingsMixin, SiteSettingsMixinInterface} from './site_settings_mixin.js';
import {RawSiteException} from './site_settings_prefs_browser_proxy.js';


/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 * @implements {SiteSettingsMixinInterface}
 * @implements {WebUIListenerBehaviorInterface}
 */
const SiteDetailsPermissionElementBase = mixinBehaviors(
    [I18nBehavior, WebUIListenerBehavior], SiteSettingsMixin(PolymerElement));

/** @polymer */
export class SiteDetailsPermissionElement extends
    SiteDetailsPermissionElementBase {
  static get is() {
    return 'site-details-permission';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * If this is a sound content setting, then this controls whether it
       * should use "Automatic" instead of "Allow" as the default setting
       * allow label.
       */
      useAutomaticLabel: {type: Boolean, value: false},

      /**
       * The site that this widget is showing details for, or null if this
       * widget should be hidden.
       * @type {RawSiteException}
       */
      site: Object,

      /**
       * The default setting for this permission category.
       * @type {ContentSetting}
       * @private
       */
      defaultSetting_: String,

      label: String,

      icon: String,

    };
  }

  static get observers() {
    return ['siteChanged_(site)'];
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    this.addWebUIListener(
        'contentSettingCategoryChanged',
        this.onDefaultSettingChanged_.bind(this));
  }

  /**
   * Updates the drop-down value after |site| has changed. If |site| is null,
   * this element will hide.
   * @param {?RawSiteException} site The site to display.
   * @private
   */
  siteChanged_(site) {
    if (!site) {
      return;
    }

    if (site.source === SiteSettingSource.DEFAULT) {
      this.defaultSetting_ = site.setting;
      this.$.permission.value = ContentSetting.DEFAULT;
    } else {
      // The default setting is unknown, so consult the C++ backend for it.
      this.updateDefaultPermission_(site);
      this.$.permission.value = site.setting;
    }

    if (this.isNonDefaultAsk_(site.setting, site.source)) {
      assert(
          this.$.permission.value === ContentSetting.ASK,
          '\'Ask\' should only show up when it\'s currently selected.');
    }
  }

  /**
   * Updates the default permission setting for this permission category.
   * @param {!RawSiteException} site The site to display.
   * @private
   */
  updateDefaultPermission_(site) {
    this.browserProxy.getDefaultValueForContentType(this.category)
        .then((defaultValue) => {
          this.defaultSetting_ = defaultValue.setting;
        });
  }

  /**
   * Handles the category permission changing for this origin.
   * @param {!ContentSettingsTypes} category The permission category
   *     that has changed default permission.
   * @private
   */
  onDefaultSettingChanged_(category) {
    if (category === this.category) {
      this.updateDefaultPermission_(this.site);
    }
  }

  /**
   * Handles the category permission changing for this origin.
   * @private
   */
  onPermissionSelectionChange_() {
    this.browserProxy.setOriginPermissions(
        this.site.origin, this.category, this.$.permission.value);
  }

  /**
   * Returns if we should use the custom labels for the sound type.
   * @param {!ContentSettingsTypes} category The permission type.
   * @return {boolean}
   * @private
   */
  useCustomSoundLabels_(category) {
    return category === ContentSettingsTypes.SOUND;
  }

  /**
   * Updates the string used for this permission category's default setting.
   * @param {!ContentSetting} defaultSetting Value of the default
   *     setting for this permission category.
   * @param {!ContentSettingsTypes} category The permission type.
   * @param {boolean} useAutomaticLabel Whether to use the automatic label
   *     if the default setting value is allow.
   * @return {string}
   * @private
   */
  defaultSettingString_(defaultSetting, category, useAutomaticLabel) {
    if (defaultSetting === undefined || category === undefined ||
        useAutomaticLabel === undefined) {
      return '';
    }

    if (defaultSetting === ContentSetting.ASK ||
        defaultSetting === ContentSetting.IMPORTANT_CONTENT) {
      return this.i18n('siteSettingsActionAskDefault');
    } else if (defaultSetting === ContentSetting.ALLOW) {
      if (this.useCustomSoundLabels_(category) && useAutomaticLabel) {
        return this.i18n('siteSettingsActionAutomaticDefault');
      }
      return this.i18n('siteSettingsActionAllowDefault');
    } else if (defaultSetting === ContentSetting.BLOCK) {
      if (this.useCustomSoundLabels_(category)) {
        return this.i18n('siteSettingsActionMuteDefault');
      }
      return this.i18n('siteSettingsActionBlockDefault');
    }
    assertNotReached(
        `No string for ${this.category}'s default of ${defaultSetting}`);
  }

  /**
   * Updates the string used for this permission category's block setting.
   * @param {!ContentSettingsTypes} category The permission type.
   * @param {string} blockString 'Block' label.
   * @param {string} muteString 'Mute' label.
   * @return {string}
   * @private
   */
  blockSettingString_(category, blockString, muteString) {
    if (this.useCustomSoundLabels_(category)) {
      return muteString;
    }
    return blockString;
  }

  /**
   * Returns true if |this| should be hidden.
   * @private
   */
  shouldHideCategory_() {
    return !this.site;
  }

  /**
   * Returns true if there's a string to display that provides more information
   * about this permission's setting. Currently, this only gets called when
   * |this.site| is updated.
   * @param {!SiteSettingSource} source The source of the permission.
   * @param {!ContentSettingsTypes} category The permission type.
   * @param {!ContentSetting} setting The permission setting.
   * @param {?string} settingDetail A sublabel for the permission.
   * @return {boolean} Whether the permission will have a source string to
   *     display.
   * @private
   */
  hasPermissionInfoString_(source, category, setting, settingDetail) {
    // This method assumes that an empty string will be returned for categories
    // that have no permission info string.
    return this.permissionInfoString_(
               source, category, setting, settingDetail,
               // Set all permission info string arguments as null. This is OK
               // because there is no need to know what the information string
               // will be, just whether there is one or not.
               null, null, null, null, null, null, null, null, null, null, null,
               null) !== '';
  }

  /**
   * Checks if there's a additional information to display, and returns the
   * class name to apply to permissions if so.
   * @param {!SiteSettingSource} source The source of the permission.
   * @param {!ContentSettingsTypes} category The permission type.
   * @param {!ContentSetting} setting The permission setting.
   * @param {?string} settingDetail A sublabel for the permission.
   * @return {string} CSS class applied when there is an additional description
   *     string.
   * @private
   */
  permissionInfoStringClass_(source, category, setting, settingDetail) {
    return this.hasPermissionInfoString_(
               source, category, setting, settingDetail) ?
        'two-line' :
        '';
  }

  /**
   * Returns true if this permission can be controlled by the user.
   * @param {!SiteSettingSource} source The source of the permission.
   * @return {boolean}
   * @private
   */
  isPermissionUserControlled_(source) {
    return !(
        source === SiteSettingSource.ALLOWLIST ||
        source === SiteSettingSource.POLICY ||
        source === SiteSettingSource.EXTENSION ||
        source === SiteSettingSource.KILL_SWITCH ||
        source === SiteSettingSource.INSECURE_ORIGIN);
  }

  /**
   * Returns true if the 'allow' option should be shown.
   * @param {!ContentSettingsTypes} category The permission type.
   * @return {boolean}
   * @private
   */
  showAllowedSetting_(category) {
    return !(
        category === ContentSettingsTypes.SERIAL_PORTS ||
        category === ContentSettingsTypes.USB_DEVICES ||
        category === ContentSettingsTypes.BLUETOOTH_SCANNING ||
        category === ContentSettingsTypes.FILE_SYSTEM_WRITE ||
        category === ContentSettingsTypes.HID_DEVICES ||
        category === ContentSettingsTypes.BLUETOOTH_DEVICES);
  }

  /**
   * Returns true if the 'ask' option should be shown.
   * @param {!ContentSettingsTypes} category The permission type.
   * @param {!ContentSetting} setting The setting of the permission.
   * @param {!SiteSettingSource} source The source of the permission.
   * @return {boolean}
   * @private
   */
  showAskSetting_(category, setting, source) {
    // For chooser-based permissions 'ask' takes the place of 'allow'.
    if (category === ContentSettingsTypes.SERIAL_PORTS ||
        category === ContentSettingsTypes.USB_DEVICES ||
        category === ContentSettingsTypes.HID_DEVICES ||
        category === ContentSettingsTypes.BLUETOOTH_DEVICES) {
      return true;
    }

    // For Bluetooth scanning permission and File System write permission
    // 'ask' takes the place of 'allow'.
    if (category === ContentSettingsTypes.BLUETOOTH_SCANNING ||
        category === ContentSettingsTypes.FILE_SYSTEM_WRITE) {
      return true;
    }

    return this.isNonDefaultAsk_(setting, source);
  }

  /**
   * Returns true if the permission is set to a non-default 'ask'. Currently,
   * this only gets called when |this.site| is updated.
   * @param {!ContentSetting} setting The setting of the permission.
   * @param {!SiteSettingSource} source The source of the permission.
   * @private
   */
  isNonDefaultAsk_(setting, source) {
    if (setting !== ContentSetting.ASK ||
        source === SiteSettingSource.DEFAULT) {
      return false;
    }

    assert(
        source === SiteSettingSource.EXTENSION ||
            source === SiteSettingSource.POLICY ||
            source === SiteSettingSource.PREFERENCE,
        'Only extensions, enterprise policy or preferences can change ' +
            'the setting to ASK.');
    return true;
  }

  /**
   * Updates the information string for the current permission.
   * Currently, this only gets called when |this.site| is updated.
   * @param {!SiteSettingSource} source The source of the permission.
   * @param {!ContentSettingsTypes} category The permission type.
   * @param {!ContentSetting} setting The permission setting.
   * @param {?string} settingDetail If non-empty, the string to display as the
   *     permission info. This overrides other calculations made by this
   *     function, and is used for situations where extra data about the
   *     permission is required to compose the substring.
   * @param {?string} allowlistString The string to show if the permission is
   *     allowlisted.
   * @param {?string} adsBlacklistString The string to show if the site is
   *     blacklisted for showing bad ads.
   * @param {?string} adsBlockString The string to show if ads are blocked, but
   *     the site is not blacklisted.
   * @param {?string} embargoString
   * @param {?string} insecureOriginString
   * @param {?string} killSwitchString
   * @param {?string} extensionAllowString
   * @param {?string} extensionBlockString
   * @param {?string} extensionAskString
   * @param {?string} policyAllowString
   * @param {?string} policyBlockString
   * @param {?string} policyAskString
   * @return {?string} The permission information string to display in the HTML.
   * @private
   */
  permissionInfoString_(
      source, category, setting, settingDetail, allowlistString,
      adsBlacklistString, adsBlockString, embargoString, insecureOriginString,
      killSwitchString, extensionAllowString, extensionBlockString,
      extensionAskString, policyAllowString, policyBlockString,
      policyAskString) {
    if (source === undefined || category === undefined ||
        setting === undefined) {
      return null;
    }

    if (settingDetail) {
      // For now, settingDetail is only used for file extensions.
      // TODO(estade): assert in the other direction as well: the FILE_HANDLING
      // category should always have detail text.
      assert(category === ContentSettingsTypes.FILE_HANDLING);
      return settingDetail;
    }

    /** @type {Object<!ContentSetting, ?string>} */
    const extensionStrings = {};
    extensionStrings[ContentSetting.ALLOW] = extensionAllowString;
    extensionStrings[ContentSetting.BLOCK] = extensionBlockString;
    extensionStrings[ContentSetting.ASK] = extensionAskString;

    /** @type {Object<!ContentSetting, ?string>} */
    const policyStrings = {};
    policyStrings[ContentSetting.ALLOW] = policyAllowString;
    policyStrings[ContentSetting.BLOCK] = policyBlockString;
    policyStrings[ContentSetting.ASK] = policyAskString;

    if (source === SiteSettingSource.ALLOWLIST) {
      return allowlistString;
    } else if (source === SiteSettingSource.ADS_FILTER_BLACKLIST) {
      assert(
          ContentSettingsTypes.ADS === category,
          'The ads filter blacklist only applies to Ads.');
      return adsBlacklistString;
    } else if (
        category === ContentSettingsTypes.ADS &&
        setting === ContentSetting.BLOCK) {
      return adsBlockString;
    } else if (source === SiteSettingSource.EMBARGO) {
      assert(
          ContentSetting.BLOCK === setting,
          'Embargo is only used to block permissions.');
      return embargoString;
    } else if (source === SiteSettingSource.EXTENSION) {
      return extensionStrings[setting];
    } else if (source === SiteSettingSource.INSECURE_ORIGIN) {
      assert(
          ContentSetting.BLOCK === setting,
          'Permissions can only be blocked due to insecure origins.');
      return insecureOriginString;
    } else if (source === SiteSettingSource.KILL_SWITCH) {
      assert(
          ContentSetting.BLOCK === setting,
          'The permissions kill switch can only be used to block permissions.');
      return killSwitchString;
    } else if (source === SiteSettingSource.POLICY) {
      return policyStrings[setting];
    } else if (
        source === SiteSettingSource.DEFAULT ||
        source === SiteSettingSource.PREFERENCE) {
      return '';
    }
    assertNotReached(`No string for ${category} setting source '${source}'`);
  }
}

customElements.define(
    SiteDetailsPermissionElement.is, SiteDetailsPermissionElement);
