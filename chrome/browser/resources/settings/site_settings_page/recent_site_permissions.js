// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/policy/cr_tooltip_icon.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';
import '../settings_shared_css.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {focusWithoutInk} from 'chrome://resources/js/cr/ui/focus_without_ink.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {routes} from '../route.js';
import {Route, RouteObserverBehavior, Router} from '../router.m.js';
import {AllSitesAction2, ContentSetting, ContentSettingsTypes, SiteSettingSource} from '../site_settings/constants.js';
import {SiteSettingsBehavior} from '../site_settings/site_settings_behavior.js';
import {RawSiteException, RecentSitePermissions} from '../site_settings/site_settings_prefs_browser_proxy.js';

Polymer({
  is: 'settings-recent-site-permissions',

  _template: html`{__html_template__}`,

  behaviors: [
    RouteObserverBehavior,
    SiteSettingsBehavior,
    WebUIListenerBehavior,
    I18nBehavior,
  ],

  properties: {
    /** @type boolean */
    noRecentPermissions: {
      type: Boolean,
      computed: 'computeNoRecentPermissions_(recentSitePermissionsList_)',
      notify: true,
    },

    /**
     * @private {boolean}
     */
    shouldFocusAfterPopulation_: Boolean,

    /**
     * List of recent site permissions grouped by source.
     * @type {!Array<RecentSitePermissions>}
     * @private
     */
    recentSitePermissionsList_: {
      type: Array,
      value: () => [],
    },

    /** @type {!Map<string, (string|Function)>} */
    focusConfig: {
      type: Object,
      observer: 'focusConfigChanged_',
    },
  },

  /**
   * When navigating to a site details sub-page, |lastSelected_| holds the
   * origin and incognito bit associated with the link that sent the user there,
   * as well as the index in recent permission list for that entry. This allows
   * for an intelligent re-focus upon a back navigation.
   * @private {!{origin: string, incognito: boolean, index: number}|null}
   */
  lastSelected_: null,

  /**
   * @param {!Map<string, string>} newConfig
   * @param {?Map<string, string>} oldConfig
   * @private
   */
  focusConfigChanged_(newConfig, oldConfig) {
    // focusConfig is set only once on the parent, so this observer should
    // only fire once.
    assert(!oldConfig);

    this.focusConfig.set(
        routes.SITE_SETTINGS_SITE_DETAILS.path + '_' +
            routes.SITE_SETTINGS.path,
        () => {
          this.shouldFocusAfterPopulation_ = true;
        });
  },

  /**
   * Reload the site recent site permission list whenever the user navigates
   * to the site settings page.
   * @param {!Route} currentRoute
   * @protected
   */
  currentRouteChanged(currentRoute) {
    if (currentRoute.path === routes.SITE_SETTINGS.path) {
      this.populateList_();
    }
  },

  /** @override */
  ready() {
    this.addWebUIListener(
        'onIncognitoStatusChanged', this.onIncognitoStatusChanged_.bind(this));
    this.browserProxy.updateIncognitoStatus();
  },

  /**
   * Perform internationalization for the given content settings type.
   * @param {string} contentSettingsType
   * @return {string} The localised content setting type string
   * @private
   */
  getI18nContentTypeString_(contentSettingsType) {
    switch (contentSettingsType) {
      case ContentSettingsTypes.COOKIES:
        return this.i18n('siteSettingsCookies');
      case ContentSettingsTypes.IMAGES:
        return this.i18n('siteSettingsImages');
      case ContentSettingsTypes.JAVASCRIPT:
        return this.i18n('siteSettingsJavascript');
      case ContentSettingsTypes.SOUND:
        return this.i18n('siteSettingsSound');
      case ContentSettingsTypes.POPUPS:
        return this.i18n('siteSettingsPopups');
      case ContentSettingsTypes.GEOLOCATION:
        return this.i18n('siteSettingsLocation');
      case ContentSettingsTypes.NOTIFICATIONS:
        return this.i18n('siteSettingsNotifications');
      case ContentSettingsTypes.MIC:
        return this.i18n('siteSettingsMic');
      case ContentSettingsTypes.CAMERA:
        return this.i18n('siteSettingsCamera');
      case ContentSettingsTypes.PROTOCOL_HANDLERS:
        return this.i18n('siteSettingsHandlers');
      case ContentSettingsTypes.UNSANDBOXED_PLUGINS:
        return this.i18n('siteSettingsUnsandboxedPlugins');
      case ContentSettingsTypes.AUTOMATIC_DOWNLOADS:
        return this.i18n('siteSettingsAutomaticDownloads');
      case ContentSettingsTypes.BACKGROUND_SYNC:
        return this.i18n('siteSettingsBackgroundSync');
      case ContentSettingsTypes.MIDI_DEVICES:
        return this.i18n('siteSettingsMidiDevices');
      case ContentSettingsTypes.USB_DEVICES:
        return this.i18n('siteSettingsUsbDevices');
      case ContentSettingsTypes.SERIAL_PORTS:
        return this.i18n('siteSettingsSerialPorts');
      case ContentSettingsTypes.BLUETOOTH_DEVICES:
        return this.i18n('siteSettingsBluetoothDevices');
      case ContentSettingsTypes.ZOOM_LEVELS:
        return this.i18n('siteSettingsZoomLevels');
      case ContentSettingsTypes.PROTECTED_CONTENT:
        return this.i18n('siteSettingsProtectedContent');
      case ContentSettingsTypes.ADS:
        return this.i18n('siteSettingsAds');
      case ContentSettingsTypes.CLIPBOARD:
        return this.i18n('siteSettingsClipboard');
      case ContentSettingsTypes.SENSORS:
        return this.i18n('siteSettingsSensors');
      case ContentSettingsTypes.PAYMENT_HANDLER:
        return this.i18n('siteSettingsPaymentHandler');
      case ContentSettingsTypes.MIXEDSCRIPT:
        return this.i18n('siteSettingsInsecureContent');
      case ContentSettingsTypes.BLUETOOTH_SCANNING:
        return this.i18n('siteSettingsBluetoothScanning');
      case ContentSettingsTypes.FILE_SYSTEM_WRITE:
        return this.i18n('siteSettingsFileSystemWrite');
      case ContentSettingsTypes.HID_DEVICES:
        return this.i18n('siteSettingsHidDevices');
      case ContentSettingsTypes.AR:
        return this.i18n('siteSettingsAr');
      case ContentSettingsTypes.VR:
        return this.i18n('siteSettingsVr');
      case ContentSettingsTypes.WINDOW_PLACEMENT:
        return this.i18n('siteSettingsWindowPlacement');
      case ContentSettingsTypes.FONT_ACCESS:
        return this.i18n('fonts');
      case ContentSettingsTypes.IDLE_DETECTION:
        return this.i18n('siteSettingsIdleDetection');
      default:
        return '';
    }
  },

  /**
   * Return the display string representing a permission change.
   * @param {!RawSiteException} rawSiteException
   * @param {boolean} sentenceStart Whether the returned string will start a
   *     sentence.
   * @return {string} The string representing the permission change.
   * @private
   */
  getI18nPermissionChangeString_({setting, source, type}, sentenceStart) {
    let change;
    if (setting === ContentSetting.ALLOW) {
      change = 'Allowed';
    } else if (setting === ContentSetting.BLOCK) {
      if (source === SiteSettingSource.EMBARGO) {
        change = 'Autoblocked';
      } else if (source === SiteSettingSource.PREFERENCE) {
        change = 'Blocked';
      } else {
        return '';
      }
    }
    const suffix = sentenceStart ? 'SentenceStart' : '';
    const msgId = `recentPermissionChange${change}${suffix}`;
    return this.i18n(msgId, this.getI18nContentTypeString_(type));
  },

  /**
   * Returns a user-friendly name for the origin a set of recent permissions
   * is associated with.
   * @param {!RecentSitePermissions} recentSitePermissions
   * @return {string} The user-friendly name.
   * @private
   */
  getDisplayName_(recentSitePermissions) {
    const url = this.toUrl(recentSitePermissions.origin);
    return url.host;
  },

  /**
   * Returns the site scheme for the origin of a set of recent permissions.
   * @param {!RecentSitePermissions} recentSitePermissions
   * @return {string} The site scheme.
   * @private
   */
  getSiteScheme_({origin}) {
    const url = this.toUrl(origin);
    const scheme = url.protocol.slice(0, -1);
    return scheme === 'https' ? '' : scheme;
  },

  /**
   * Returns the display text which describes the set of recent permissions.
   * @param {!RecentSitePermissions} recentSitePermissions
   * @return {string} The display string for set of site permissions.
   * @private
   */
  getPermissionsText_({recentPermissions, incognito}) {
    const displayStrings = recentPermissions.map(
        (rp, i) => this.getI18nPermissionChangeString_(rp, i === 0));

    if (recentPermissions.length === 1 && !incognito) {
      return displayStrings[0];
    }

    const itemsPart = [
      'OneItem',
      'TwoItems',
      'ThreeItems',
      'OverThreeItems',
    ][Math.min(recentPermissions.length, 4) - 1];
    const suffix = incognito ? 'Incognito' : '';
    const i18nStringID = `recentPermissions${itemsPart}${suffix}`;

    return this.i18n(i18nStringID, ...displayStrings);
  },

  /**
   * Returns the correct class to apply depending on this recent site
   * permissions entry based on the index.
   * @param {number} index
   * @return {string} The CSS class corresponding to the provided index
   * @private
   */
  getClassForIndex_(index) {
    return index === 0 ? 'first' : '';
  },

  /**
   * Returns true if there are no recent site permissions to display
   * @return {boolean}
   * @private
   */
  computeNoRecentPermissions_() {
    return this.recentSitePermissionsList_.length === 0;
  },

  /**
   * Called for when incognito is enabled or disabled. Only called on change
   * (opening N incognito windows only fires one message). Another message is
   * sent when the *last* incognito window closes.
   * @param {boolean} hasIncognito
   * @private
   */
  onIncognitoStatusChanged_(hasIncognito) {
    // We're only interested in the case where we transition out of incognito
    // and we are currently displaying an incognito entry.
    if (hasIncognito === false &&
        this.recentSitePermissionsList_.some(p => p.incognito)) {
      this.populateList_();
    }
  },

  /**
   * A handler for selecting a recent site permissions entry.
   * @param {!{model: !{item: !RecentSitePermissions, index: number}}} e
   * @private
   */
  onRecentSitePermissionClick_(e) {
    const origin = this.recentSitePermissionsList_[e.model.index].origin;
    Router.getInstance().navigateTo(
        routes.SITE_SETTINGS_SITE_DETAILS, new URLSearchParams({site: origin}));
    this.browserProxy.recordAction(AllSitesAction2.ENTER_SITE_DETAILS);
    this.lastSelected_ = {
      index: e.model.index,
      origin: e.model.item.origin,
      incognito: e.model.item.incognito,
    };
  },

  /**
   * @param {!Event} e
   * @private
   */
  onShowIncognitoTooltip_(e) {
    e.stopPropagation();

    const target = e.target;
    const tooltip = this.$.tooltip;
    tooltip.target = target;
    /** @type {{updatePosition: Function}} */ (tooltip).updatePosition();
    const hide = () => {
      /** @type {{hide: Function}} */ (tooltip).hide();
      target.removeEventListener('mouseleave', hide);
      target.removeEventListener('blur', hide);
      target.removeEventListener('tap', hide);
      tooltip.removeEventListener('mouseenter', hide);
    };
    target.addEventListener('mouseleave', hide);
    target.addEventListener('blur', hide);
    target.addEventListener('tap', hide);
    tooltip.addEventListener('mouseenter', hide);

    tooltip.show();
  },

  /**
   * Called after the list has finished populating and |lastSelected_| contains
   * a valid entry that should attempt to be focused. If lastSelected_ cannot
   * be found the index where it used to be is focused. This may result in
   * focusing another link arrow, or an incognito information icon. If the
   * recent permission list is empty, focus is lost.
   * @private
   */
  focusLastSelected_() {
    if (this.noRecentPermissions) {
      return;
    }
    const currentIndex =
        this.recentSitePermissionsList_.findIndex((permissions) => {
          return permissions.origin === this.lastSelected_.origin &&
              permissions.incognito === this.lastSelected_.incognito;
        });

    const fallbackIndex = Math.min(
        this.lastSelected_.index, this.recentSitePermissionsList_.length - 1);

    const index = currentIndex > -1 ? currentIndex : fallbackIndex;

    if (this.recentSitePermissionsList_[index].incognito) {
      focusWithoutInk(assert(/** @type {{getFocusableElement: Function}} */ (
                                 this.$$(`#incognitoInfoIcon_${index}`))
                                 .getFocusableElement()));
    } else {
      focusWithoutInk(assert(this.$$(`#siteEntryButton_${index}`)));
    }
  },

  /**
   * Retrieve the list of recently changed permissions and implicitly trigger
   * the update of the display list.
   * @private
   */
  async populateList_() {
    this.recentSitePermissionsList_ =
        await this.browserProxy.getRecentSitePermissions(
            this.getCategoryList(), 3);
  },

  /**
   * Called when the dom-repeat DOM has changed. This allows updating the
   * focused element after the elements have been adjusted.
   * @private
   */
  onDomChange_() {
    if (this.shouldFocusAfterPopulation_) {
      this.focusLastSelected_();
      this.shouldFocusAfterPopulation_ = false;
    }
  },
});
