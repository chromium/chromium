// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/icons.m.js';
import '../icons.js';
import '../settings_shared_css.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {focusWithoutInk} from 'chrome://resources/js/cr/ui/focus_without_ink.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {PrefsBehavior} from '../prefs/prefs_behavior.js';
import {Route, Router} from '../router.js';
import {ContentSetting, ContentSettingsTypes, NotificationSetting} from '../site_settings/constants.js';
import {SiteSettingsPrefsBrowserProxy, SiteSettingsPrefsBrowserProxyImpl} from '../site_settings/site_settings_prefs_browser_proxy.js';

/**
 * @typedef{{
 *   route: !Route,
 *   id: ContentSettingsTypes,
 *   label: string,
 *   icon: (string|undefined),
 *   enabledLabel: (string|undefined),
 *   disabledLabel: (string|undefined),
 *   otherLabel: (string|undefined),
 *   shouldShow: function():boolean,
 * }}
 */
export let CategoryListItem;

/**
 * @param {string} setting Value from ContentSetting.
 * @param {string} enabled Non-block label ('feature X not allowed').
 * @param {string} disabled Block label (likely just, 'Blocked').
 * @param {?string} other Tristate value (maybe, 'session only').
 */
export function defaultSettingLabel(setting, enabled, disabled, other) {
  if (setting === ContentSetting.BLOCK) {
    return disabled;
  }
  if (setting === ContentSetting.ALLOW) {
    return enabled;
  }

  return other || enabled;
}

Polymer({
  is: 'settings-site-settings-list',

  _template: html`{__html_template__}`,

  behaviors: [WebUIListenerBehavior, I18nBehavior, PrefsBehavior],

  properties: {
    /** @type {!Array<!CategoryListItem>} */
    categoryList: Array,

    /** @type {!Map<string, (string|Function)>} */
    focusConfig: {
      type: Object,
      observer: 'focusConfigChanged_',
    },
  },

  observers: [
    // The prefs object is only populated for the instance of this element
    // which contains the notifications link row, avoiding non-actionable
    // firing of the observer.
    'updateNotificationsLabel_(prefs.generated.notification.*)'
  ],

  /** @type {?SiteSettingsPrefsBrowserProxy} */
  browserProxy: null,

  /**
   * @param {!Map<string, string>} newConfig
   * @param {?Map<string, string>} oldConfig
   * @private
   */
  focusConfigChanged_(newConfig, oldConfig) {
    // focusConfig is set only once on the parent, so this observer should
    // only fire once.
    assert(!oldConfig);

    // Populate the |focusConfig| map of the parent <settings-animated-pages>
    // element, with additional entries that correspond to subpage trigger
    // elements residing in this element's Shadow DOM.
    for (const item of this.categoryList) {
      this.focusConfig.set(item.route.path, () => this.async(() => {
        focusWithoutInk(assert(this.$$(`#${item.id}`)));
      }));
    }
  },

  /** @override */
  ready() {
    this.browserProxy_ = SiteSettingsPrefsBrowserProxyImpl.getInstance();

    Promise
        .all(this.categoryList.map(
            item => this.refreshDefaultValueLabel_(item.id)))
        .then(() => {
          this.fire('site-settings-list-labels-updated-for-testing');
        });

    this.addWebUIListener(
        'contentSettingCategoryChanged',
        this.refreshDefaultValueLabel_.bind(this));

    const hasProtocolHandlers = this.categoryList.some(item => {
      return item.id === ContentSettingsTypes.PROTOCOL_HANDLERS;
    });

    if (hasProtocolHandlers) {
      // The protocol handlers have a separate enabled/disabled notifier.
      this.addWebUIListener('setHandlersEnabled', enabled => {
        this.updateDefaultValueLabel_(
            ContentSettingsTypes.PROTOCOL_HANDLERS,
            enabled ? ContentSetting.ALLOW : ContentSetting.BLOCK);
      });
      this.browserProxy_.observeProtocolHandlersEnabledState();
    }

    const hasCookies = this.categoryList.some(item => {
      return item.id === ContentSettingsTypes.COOKIES;
    });
    if (hasCookies) {
      // The cookies sub-label is provided by an update from C++.
      this.browserProxy_.getCookieSettingDescription().then(
          this.updateCookiesLabel_.bind(this));
      this.addWebUIListener(
          'cookieSettingDescriptionChanged',
          this.updateCookiesLabel_.bind(this));
    }
  },

  /**
   * @param {!ContentSettingsTypes} category The category to refresh
   *     (fetch current value + update UI)
   * @return {!Promise<void>} A promise firing after the label has been
   *     updated.
   * @private
   */
  refreshDefaultValueLabel_(category) {
    // Default labels are not applicable to ZOOM_LEVELS, PDF or
    // PROTECTED_CONTENT
    if (category === ContentSettingsTypes.ZOOM_LEVELS ||
        category === ContentSettingsTypes.PROTECTED_CONTENT ||
        category === 'pdfDocuments') {
      return Promise.resolve();
    }

    if (category === ContentSettingsTypes.COOKIES) {
      // Updates to the cookies label are handled by the
      // cookieSettingDescriptionChanged event listener.
      return Promise.resolve();
    }

    if (category === ContentSettingsTypes.NOTIFICATIONS) {
      // Updates to the notifications label are handled by a preference
      // observer.
      return Promise.resolve();
    }

    return this.browserProxy_.getDefaultValueForContentType(category).then(
        defaultValue => {
          this.updateDefaultValueLabel_(category, defaultValue.setting);
        });
  },

  /**
   * Updates the DOM for the given |category| to display a label that
   * corresponds to the given |setting|.
   * @param {!ContentSettingsTypes} category
   * @param {!ContentSetting} setting
   * @private
   */
  updateDefaultValueLabel_(category, setting) {
    const element = this.$$(`#${category}`);
    if (!element) {
      // |category| is not part of this list.
      return;
    }

    const index = this.$$('dom-repeat').indexForElement(element);
    const dataItem = this.categoryList[index];
    this.set(
        `categoryList.${index}.subLabel`,
        defaultSettingLabel(
            setting,
            dataItem.enabledLabel ? this.i18n(dataItem.enabledLabel) : '',
            dataItem.disabledLabel ? this.i18n(dataItem.disabledLabel) : '',
            dataItem.otherLabel ? this.i18n(dataItem.otherLabel) : null));
  },

  /**
   * Update the cookies link row label when the cookies setting description
   * changes.
   * @param {string} label
   * @private
   */
  updateCookiesLabel_(label) {
    const index = this.$$('dom-repeat').indexForElement(this.$$('#cookies'));
    this.set(`categoryList.${index}.subLabel`, label);
  },

  /**
   * Update the notifications link row label when the notifications setting
   * description changes.
   * @private
   */
  updateNotificationsLabel_() {
    const redesignEnabled =
        loadTimeData.getBoolean('enableContentSettingsRedesign');

    const state = this.getPref('generated.notification').value;
    const index = this.categoryList.map(e => e.id).indexOf(
        ContentSettingsTypes.NOTIFICATIONS);

    // updateNotificationsLabel_ should only be called for the
    // site-settings-list instance which contains notifications.
    assert(index !== -1);

    let label = redesignEnabled ? 'siteSettingsNotificationsBlocked' :
                                  'siteSettingsBlocked';
    if (state === NotificationSetting.ASK) {
      label = redesignEnabled ? 'siteSettingsNotificationsAllowed' :
                                'siteSettingsAskBeforeSending';
    } else if (state === NotificationSetting.QUIETER_MESSAGING) {
      label = redesignEnabled ? 'siteSettingsNotificationsPartial' :
                                'siteSettingsAskBeforeSending';
    }
    this.set(`categoryList.${index}.subLabel`, this.i18n(label));
  },

  /**
   * @param {!Event} event
   * @private
   */
  onClick_(event) {
    Router.getInstance().navigateTo(this.categoryList[event.model.index].route);
  },
});
