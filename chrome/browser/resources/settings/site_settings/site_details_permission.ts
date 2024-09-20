// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'site-details-permission' handles showing the state of one permission, such
 * as Geolocation, for a given origin.
 */
import 'chrome://resources/cr_elements/md_select.css.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import '../settings_shared.css.js';
import '../settings_vars.css.js';
import '../i18n_setup.js';
import './site_details_permission_device_entry.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {ListPropertyUpdateMixin} from 'chrome://resources/cr_elements/list_property_update_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ChooserType, ContentSetting, ContentSettingsTypes, SiteSettingSource} from './constants.js';
import {getTemplate} from './site_details_permission.html.js';
import {SiteSettingsMixin} from './site_settings_mixin.js';
import type {ChooserException, RawChooserException, RawSiteException} from './site_settings_prefs_browser_proxy.js';

export interface SiteDetailsPermissionElement {
  $: {
    details: HTMLElement,
    permission: HTMLSelectElement,
    permissionItem: HTMLElement,
    permissionSecondary: HTMLElement,
  };
}

const SiteDetailsPermissionElementBase = ListPropertyUpdateMixin(
    SiteSettingsMixin(WebUiListenerMixin(I18nMixin(PolymerElement))));

export class SiteDetailsPermissionElement extends
    SiteDetailsPermissionElementBase {
  static get is() {
    return 'site-details-permission';
  }

  static get template() {
    return getTemplate();
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
       */
      site: Object,

      /**
       * The default setting for this permission category.
       */
      defaultSetting_: String,

      label: String,

      icon: String,

      /**
       * Expose ContentSetting enum to HTML bindings.
       */
      contentSettingEnum_: {
        type: Object,
        value: ContentSetting,
      },

      /**
       * Array of chooser exceptions to display in the widget.
       */
      chooserExceptions_: {
        type: Array,
        value() {
          return [];
        },
      },

      /**
       * The chooser type that this element is displaying data for.
       * See site_settings/constants.js for possible values.
       */
      chooserType: {
        type: String,
        value: ChooserType.NONE,
      },

      /**
       * If the permission for this category permission is blocked on the system
       * level, this will be populated with the key that can be used to look up
       * the warning to be shown in the UI.
       */
      systemPermissionWarningKey_: {
        type: String,
        value: null,
        observer: 'attachSystemPermissionSettingsLinkClick_',
      },
    };
  }

  static get observers() {
    return [
      'siteChanged_(site)',
      'updateChooserExceptions_(site, chooserType)',
    ];
  }

  useAutomaticLabel: boolean;
  site: RawSiteException;
  private chooserExceptions_: ChooserException[];
  chooserType: ChooserType;
  private systemPermissionWarningKey_: string;
  private defaultSetting_: ContentSetting;
  label: string;
  icon: string;

  override connectedCallback() {
    super.connectedCallback();

    this.addWebUiListener(
        'contentSettingCategoryChanged',
        (category: ContentSettingsTypes) =>
            this.onDefaultSettingChanged_(category));

    this.addWebUiListener(
        'contentSettingChooserPermissionChanged',
        (category: ContentSettingsTypes, chooserType: ChooserType) => {
          if (category === this.category && chooserType === this.chooserType) {
            this.updateChooserExceptions_();
          }
        });

    this.addWebUiListener(
        'osGlobalPermissionChanged', (messages: ContentSettingsTypes[]) => {
          this.setSystemPermissionWarningKey_(messages);
        });
  }

  /**
   * Update the chooser exception list for display.
   */
  private updateChooserExceptions_() {
    if (!this.site || this.chooserType === ChooserType.NONE) {
      return;
    }
    // TODO(crbug.com/40887747): Use a backend handler to get chooser
    // exceptions with a given origin so avoid complex logic in
    // processChooserExceptions_.
    this.browserProxy.getChooserExceptionList(this.chooserType)
        .then(exceptionList => this.processChooserExceptions_(exceptionList));
  }

  private updateOsPermissionWarning_() {
    this.browserProxy.getSystemDeniedPermissions().then(
        (messages: ContentSettingsTypes[]) => {
          this.setSystemPermissionWarningKey_(messages);
        });
  }

  /**
   * Process the chooser exception list returned from the native layer by
   * keeping the exception that is relevant to |this.site| and filtering out
   * sites of exception that doesn't match |this.site|.
   */
  private processChooserExceptions_(exceptionList: RawChooserException[]) {
    // TODO(crbug.com/40887747): Move this processing logic to the backend and
    // remove this function.
    const siteFilter = (site: RawSiteException) => {
      // Site's origin from backend will have forward slash ending,
      // hence converting it to URL and using URL.origin for
      // comparison to avoid mismatch due to the slash ending.
      const url = this.toUrl(site.origin);
      const targetUrl = this.toUrl(this.site.origin);
      if (!url || !targetUrl) {
        return false;
      }
      return site.incognito === this.site.incognito &&
          url.origin === targetUrl.origin;
    };

    const exceptions =
        exceptionList
            .filter(exception => {
              // Filters out exceptions that don't have any site matching
              // |this.site|.
              return exception.sites.some(site => siteFilter(site));
            })
            .map(exception => {
              // Filters out any site of |exception.sites| that doesn't match
              // |this.site|.
              const sites = exception.sites.filter(site => siteFilter(site))
                                .map(site => this.expandSiteException(site));
              return Object.assign(exception, {sites});
            });
    this.updateList(
        'chooserExceptions_', x => x.displayName, exceptions,
        /*identityBasedUpdate=*/ true);
  }

  /**
   * Updates the drop-down value after |site| has changed. If |site| is null,
   * this element will hide.
   * @param site The site to display.
   */
  private siteChanged_(site: RawSiteException|null) {
    if (!site) {
      return;
    }

    this.updateOsPermissionWarning_();

    if (site.source === SiteSettingSource.DEFAULT) {
      this.defaultSetting_ = site.setting;
      this.$.permission.value = ContentSetting.DEFAULT;
    } else {
      // The default setting is unknown, so consult the C++ backend for it.
      this.updateDefaultPermission_();
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
   */
  private updateDefaultPermission_() {
    this.browserProxy.getDefaultValueForContentType(this.category)
        .then((defaultValue) => {
          this.defaultSetting_ = defaultValue.setting;
        });
  }

  /**
   * Handles the category permission changing for this origin.
   * @param category The permission category that has changed default
   *     permission.
   */
  private onDefaultSettingChanged_(category: ContentSettingsTypes) {
    if (category === this.category) {
      this.updateDefaultPermission_();
    }
  }

  /**
   * Handles the category permission changing for this origin.
   */
  private onPermissionSelectionChange_() {
    this.browserProxy.setOriginPermissions(
        this.site.origin, this.category,
        this.$.permission.value as ContentSetting);
  }

  /**
   * @param category The permission type.
   * @return if we should use the custom labels for the sound type.
   */
  private useCustomSoundLabels_(category: ContentSettingsTypes): boolean {
    return category === ContentSettingsTypes.SOUND;
  }

  /**
   * Updates the string used for this permission category's default setting.
   * @param defaultSetting Value of the default setting for this permission
   *     category.
   * @param category The permission type.
   * @param useAutomaticLabel Whether to use the automatic label if the default
   *     setting value is allow.
   */
  private defaultSettingString_(
      defaultSetting: ContentSetting, category: ContentSettingsTypes,
      useAutomaticLabel: boolean): string {
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
   * @param category The permission type.
   * @param blockString 'Block' label.
   * @param muteString 'Mute' label.
   */
  private blockSettingString_(
      category: ContentSettingsTypes, blockString: string,
      muteString: string): string {
    if (this.useCustomSoundLabels_(category)) {
      return muteString;
    }
    return blockString;
  }

  /**
   * @return true if |this| should be hidden.
   */
  private shouldHideCategory_() {
    return !this.site;
  }

  /**
   * Returns true if there's a string to display that provides more information
   * about this permission's setting. Currently, this only gets called when
   * |this.site| is updated.
   * @param source The source of the permission.
   * @param category The permission type.
   * @param setting The permission setting.
   * @return Whether the permission will have a source string to display.
   */
  private hasPermissionInfoString_(
      source: SiteSettingSource, category: ContentSettingsTypes,
      setting: ContentSetting): boolean {
    // This method assumes that an empty string will be returned for categories
    // that have no permission info string.
    return String(this.permissionInfoString_(
               source, category, setting,
               // Set all permission info string arguments as null. This is OK
               // because there is no need to know what the information string
               // will be, just whether there is one or not.
               null, null, null, null, null, null,
               // <if expr="is_win and _google_chrome">
               null,
               // </if>
               null, null, null, null, null, null)) !== '';
  }

  /**
   * Checks if there's a additional information to display, and returns the
   * class name to apply to permissions if so.
   * @param source The source of the permission.
   * @param category The permission type.
   * @param setting The permission setting.
   * @return CSS class applied when there is an additional description string.
   */
  private permissionInfoStringClass_(
      source: SiteSettingSource, category: ContentSettingsTypes,
      setting: ContentSetting): string {
    return (this.hasPermissionInfoString_(source, category, setting) ||
            this.hasSystemPermissionWarning_()) ?
        'two-line' :
        '';
  }

  /**
   * @param source The source of the permission.
   * @return Whether this permission can be controlled by the user.
   */
  private isPermissionUserControlled_(source: SiteSettingSource): boolean {
    return !(source === SiteSettingSource.ALLOWLIST ||
             source === SiteSettingSource.POLICY ||
             source === SiteSettingSource.EXTENSION ||
             source === SiteSettingSource.KILL_SWITCH ||
             source === SiteSettingSource.INSECURE_ORIGIN) &&
        !this.hasSystemPermissionWarning_();
  }

  /**
   * @param category The permission type.
   * @return Whether if the 'allow' option should be shown.
   */
  private showAllowedSetting_(category: ContentSettingsTypes) {
    return !(
        category === ContentSettingsTypes.SERIAL_PORTS ||
        category === ContentSettingsTypes.USB_DEVICES ||
        category === ContentSettingsTypes.BLUETOOTH_SCANNING ||
        category === ContentSettingsTypes.FILE_SYSTEM_WRITE ||
        category === ContentSettingsTypes.HID_DEVICES ||
        category === ContentSettingsTypes.BLUETOOTH_DEVICES);
  }

  /**
   * @param category The permission type.
   * @param setting The setting of the permission.
   * @param source The source of the permission.
   * @return Whether the 'ask' option should be shown.
   */
  private showAskSetting_(
      category: ContentSettingsTypes, setting: ContentSetting,
      source: SiteSettingSource): boolean {
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
   * @param messages The message with the blocked permission types.
   * @return The key to lookup the warning. Null if the warning is not to be
   * shown.
   */
  private setSystemPermissionWarningKey_(messages: ContentSettingsTypes[]) {
    this.set(
        'systemPermissionWarningKey_', ((category: ContentSettingsTypes) => {
          // We return null as warningKey in case the category is not one of
          // the listed, as the warning in case of an OS level block is
          // supported only for camera, microphone and location permissions.
          if (!messages.includes(category)) {
            return null;
          }
          switch (category) {
            case ContentSettingsTypes.CAMERA:
              return 'siteSettingsCameraBlockedByOs';
            case ContentSettingsTypes.MIC:
              return 'siteSettingsMicrophoneBlockedByOs';
            case ContentSettingsTypes.GEOLOCATION:
              return 'siteSettingsLocationBlockedByOs';
            default:
              return null;
          }
        })(this.category));
  }

  /** Attempts to open the system permission settings. */
  private onSystemPermissionSettingsLinkClick_(event: MouseEvent) {
    // Prevents navigation to href='#'.
    event.preventDefault();
    if (this.category !== null) {
      this.browserProxy.openSystemPermissionSettings(this.category);
    }
  }

  /**
   * @param category The permission type.
   * @return The text of the warning. Null if the warning is not to be shown.
   */
  private getSystemPermissionWarning_(): TrustedHTML {
    if (this.systemPermissionWarningKey_ !== null) {
      return this.i18nAdvanced(
          this.systemPermissionWarningKey_, {tags: ['a'], attrs: ['id']});
    }
    return sanitizeInnerHtml('');
  }

  /** Attaches the click action to the anchor element. */
  private attachSystemPermissionSettingsLinkClick_() {
    const element: HTMLElement|null|undefined =
        this.shadowRoot?.querySelector('#openSystemSettingsLink');
    if (element !== null && element !== undefined) {
      element.addEventListener('click', (me: MouseEvent) => {
        this.onSystemPermissionSettingsLinkClick_(me);
      });
      // Set the correct aria label describing the link target.
      const settingsPageName: string|null = (() => {
        switch (this.category) {
          case ContentSettingsTypes.CAMERA:
            return 'Camera';
          case ContentSettingsTypes.MIC:
            return 'Microphone';
          case ContentSettingsTypes.GEOLOCATION:
            return 'Location';
          default:
            return null;
        }
      })();
      if (settingsPageName) {
        element.setAttribute(
            'aria-label', `System Settings: ${settingsPageName}`);
      }
    }
  }

  /**
   * @return The text of the warning. Null if the permission is not blocked by
   *     the OS.
   */
  private hasSystemPermissionWarning_(): boolean {
    return (this.systemPermissionWarningKey_ !== null);
  }

  /**
   * @param category The permission type.
   * @return The text of the warning. Null if the permission is not to be
   *     displayed.
   */
  private showSystemPermissionWarning_(
      source: SiteSettingSource, category: ContentSettingsTypes,
      setting: ContentSetting): boolean {
    if (this.hasPermissionInfoString_(source, category, setting)) {
      return false;
    }
    return this.hasSystemPermissionWarning_();
  }


  /**
   * Returns true if the permission is set to a non-default 'ask'. Currently,
   * this only gets called when |this.site| is updated.
   * @param setting The setting of the permission.
   * @param source The source of the permission.
   */
  private isNonDefaultAsk_(setting: ContentSetting, source: SiteSettingSource) {
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
   * @param source The source of the permission.
   * @param category The permission type.
   * @param setting The permission setting.
   * @param  allowlistString The string to show if the permission is
   *     allowlisted.
   * @param adsBlocklistString The string to show if the site is
   *     blocklisted for showing bad ads.
   * @param adsBlockString The string to show if ads are blocked, but
   *     the site is not blocklisted.
   * @return The permission information string to display in the HTML.
   */
  private permissionInfoString_(
      source: SiteSettingSource, category: ContentSettingsTypes,
      setting: ContentSetting,
      allowlistString: string|null, adsBlocklistString: string|null,
      adsBlockString: string|null, embargoString: string|null,
      insecureOriginString: string|null, killSwitchString: string|null,
      // <if expr="is_win and _google_chrome">
      protectedContentIdentifierAllowedString: string|null,
      // </if>
      extensionAllowString: string|null, extensionBlockString: string|null,
      extensionAskString: string|null, policyAllowString: string|null,
      policyBlockString: string|null,
      policyAskString: string|null): (TrustedHTML|null) {
    if (source === undefined || category === undefined ||
        setting === undefined) {
      return window.trustedTypes!.emptyHTML;
    }

    const extensionStrings: {[key: string]: string|null} = {};
    extensionStrings[ContentSetting.ALLOW] = extensionAllowString;
    extensionStrings[ContentSetting.BLOCK] = extensionBlockString;
    extensionStrings[ContentSetting.ASK] = extensionAskString;

    const policyStrings: {[key: string]: string|null} = {};
    policyStrings[ContentSetting.ALLOW] = policyAllowString;
    policyStrings[ContentSetting.BLOCK] = policyBlockString;
    policyStrings[ContentSetting.ASK] = policyAskString;

    function htmlOrNull(str: string|null): TrustedHTML|null {
      return str === null ? null : sanitizeInnerHtml(str);
    }

    if (source === SiteSettingSource.ALLOWLIST) {
      return htmlOrNull(allowlistString);
    } else if (source === SiteSettingSource.ADS_FILTER_BLACKLIST) {
      assert(
          ContentSettingsTypes.ADS === category,
          'The ads filter blocklist only applies to Ads.');
      return htmlOrNull(adsBlocklistString);
    } else if (
        category === ContentSettingsTypes.ADS &&
        setting === ContentSetting.BLOCK) {
      return htmlOrNull(adsBlockString);
    } else if (source === SiteSettingSource.EMBARGO) {
      assert(
          ContentSetting.BLOCK === setting,
          'Embargo is only used to block permissions.');
      return htmlOrNull(embargoString);
    } else if (source === SiteSettingSource.EXTENSION) {
      return htmlOrNull(extensionStrings[setting]);
    } else if (source === SiteSettingSource.INSECURE_ORIGIN) {
      assert(
          ContentSetting.BLOCK === setting,
          'Permissions can only be blocked due to insecure origins.');
      return htmlOrNull(insecureOriginString);
    } else if (source === SiteSettingSource.KILL_SWITCH) {
      assert(
          ContentSetting.BLOCK === setting,
          'The permissions kill switch can only be used to block permissions.');
      return htmlOrNull(killSwitchString);
    } else if (source === SiteSettingSource.POLICY) {
      return htmlOrNull(policyStrings[setting]);
      // <if expr="is_win and _google_chrome">
    } else if (
        category === ContentSettingsTypes.PROTECTED_CONTENT &&
        setting === ContentSetting.ALLOW) {
      return htmlOrNull(protectedContentIdentifierAllowedString);
      // </if>
    } else if (
        source === SiteSettingSource.DEFAULT ||
        source === SiteSettingSource.PREFERENCE) {
      return window.trustedTypes!.emptyHTML;
    }
    assertNotReached(`No string for ${category} setting source '${source}'`);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'site-details-permission': SiteDetailsPermissionElement;
  }
}

customElements.define(
    SiteDetailsPermissionElement.is, SiteDetailsPermissionElement);
