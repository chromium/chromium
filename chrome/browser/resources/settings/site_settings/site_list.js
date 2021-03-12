// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'site-list' shows a list of Allowed and Blocked sites for a given
 * category.
 */
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.m.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/policy/cr_policy_pref_indicator.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';
import '../settings_shared_css.js';
import './add_site_dialog.js';
import './edit_exception_dialog.js';
import './site_list_entry.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {focusWithoutInk} from 'chrome://resources/js/cr/ui/focus_without_ink.m.js';
import {ListPropertyUpdateBehavior} from 'chrome://resources/js/list_property_update_behavior.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';

// <if expr="chromeos">
import {AndroidInfoBrowserProxyImpl, AndroidSmsInfo} from './android_info_browser_proxy.js';
// </if>
import {ContentSetting, ContentSettingsTypes, INVALID_CATEGORY_SUBTYPE} from './constants.js';
import {SiteSettingsBehavior} from './site_settings_behavior.js';
import {RawSiteException, SiteException, SiteSettingsPrefsBrowserProxyImpl} from './site_settings_prefs_browser_proxy.js';


Polymer({
  is: 'site-list',

  _template: html`{__html_template__}`,

  behaviors: [
    SiteSettingsBehavior,
    WebUIListenerBehavior,
    ListPropertyUpdateBehavior,
  ],

  properties: {
    /**
     * Some content types (like Location) do not allow the user to manually
     * edit the exception list from within Settings.
     */
    readOnlyList: {
      type: Boolean,
      value: false,
    },

    categoryHeader: String,

    /** @private */
    enableContentSettingsRedesign_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('enableContentSettingsRedesign');
      }
    },

    /**
     * The site serving as the model for the currently open action menu.
     * @private {?SiteException}
     */
    actionMenuSite_: Object,

    /**
     * Whether the "edit exception" dialog should be shown.
     * @private
     */
    showEditExceptionDialog_: Boolean,

    /**
     * Array of sites to display in the widget.
     * @type {!Array<SiteException>}
     */
    sites: {
      type: Array,
      value() {
        return [];
      },
    },

    /**
     * The type of category this widget is displaying data for. Normally
     * either 'allow' or 'block', representing which sites are allowed or
     * blocked respectively.
     */
    categorySubtype: {
      type: String,
      value: INVALID_CATEGORY_SUBTYPE,
    },

    /** @private */
    hasIncognito_: Boolean,

    /**
     * Whether to show the Add button next to the header.
     * @private
     */
    showAddSiteButton_: {
      type: Boolean,
      computed: 'computeShowAddSiteButton_(readOnlyList, category, ' +
          'categorySubtype)',
    },

    /** @private */
    showAddSiteDialog_: Boolean,

    /**
     * Whether to show the Allow action in the action menu.
     * @private
     */
    showAllowAction_: Boolean,

    /**
     * Whether to show the Block action in the action menu.
     * @private
     */
    showBlockAction_: Boolean,

    /**
     * Whether to show the 'Clear on exit' action in the action
     * menu.
     * @private
     */
    showSessionOnlyAction_: Boolean,

    /**
     * All possible actions in the action menu.
     * @private
     */
    actions_: {
      readOnly: true,
      type: Object,
      values: {
        ALLOW: 'Allow',
        BLOCK: 'Block',
        RESET: 'Reset',
        SESSION_ONLY: 'SessionOnly',
      }
    },

    /** @private */
    lastFocused_: Object,

    /** @private */
    listBlurred_: Boolean,

    /** @private */
    tooltipText_: String,

    searchFilter: String,
  },

  // <if expr="chromeos">
  /**
   * Android messages info object containing messages feature state and
   * exception origin.
   * @private {?AndroidSmsInfo}
   */
  androidSmsInfo_: null,
  // </if>

  /**
   * The element to return focus to, when the currently active dialog is closed.
   * @private {?HTMLElement}
   */
  activeDialogAnchor_: null,

  observers: ['configureWidget_(category, categorySubtype)'],

  /** @override */
  ready() {
    this.addWebUIListener(
        'contentSettingSitePermissionChanged',
        this.siteWithinCategoryChanged_.bind(this));
    this.addWebUIListener(
        'onIncognitoStatusChanged', this.onIncognitoStatusChanged_.bind(this));
    // <if expr="chromeos">
    this.addWebUIListener('settings.onAndroidSmsInfoChange', (info) => {
      this.androidSmsInfo_ = info;
      this.populateList_();
    });
    // </if>
    this.browserProxy.updateIncognitoStatus();
  },

  /**
   * Called when a site changes permission.
   * @param {string} category The category of the site that changed.
   * @param {string} site The site that changed.
   * @private
   */
  siteWithinCategoryChanged_(category, site) {
    if (category === this.category) {
      this.configureWidget_();
    }
  },

  /**
   * Called for each site list when incognito is enabled or disabled. Only
   * called on change (opening N incognito windows only fires one message).
   * Another message is sent when the *last* incognito window closes.
   * @private
   */
  onIncognitoStatusChanged_(hasIncognito) {
    this.hasIncognito_ = hasIncognito;

    // The SESSION_ONLY list won't have any incognito exceptions. (Minor
    // optimization, not required).
    if (this.categorySubtype === ContentSetting.SESSION_ONLY) {
      return;
    }

    // A change notification is not sent for each site. So we repopulate the
    // whole list when the incognito profile is created or destroyed.
    this.populateList_();
  },

  /**
   * Configures the action menu, visibility of the widget and shows the list.
   * @private
   */
  configureWidget_() {
    if (this.category === undefined) {
      return;
    }

    // The observer for All Sites fires before the attached/ready event, so
    // initialize this here.
    if (this.browserProxy_ === undefined) {
      this.browserProxy_ = SiteSettingsPrefsBrowserProxyImpl.getInstance();
    }

    this.setUpActionMenu_();

    // <if expr="not chromeos">
    this.populateList_();
    // </if>

    // <if expr="chromeos">
    this.updateAndroidSmsInfo_().then(this.populateList_.bind(this));
    // </if>

    // The Session permissions are only for cookies.
    if (this.categorySubtype === ContentSetting.SESSION_ONLY) {
      this.$.category.hidden = this.category !== ContentSettingsTypes.COOKIES;
    }
  },

  /**
   * Whether there are any site exceptions added for this content setting.
   * @return {boolean}
   * @private
   */
  hasSites_() {
    return this.sites.length > 0;
  },

  /**
   * Whether the Add Site button is shown in the header for the current category
   * and category subtype.
   * @return {boolean}
   * @private
   */
  computeShowAddSiteButton_() {
    return !(
        this.readOnlyList ||
        (this.category === ContentSettingsTypes.FILE_SYSTEM_WRITE &&
         this.categorySubtype === ContentSetting.ALLOW));
  },

  /**
   * @return {boolean}
   * @private
   */
  showNoSearchResults_() {
    return this.sites.length > 0 && this.getFilteredSites_().length === 0;
  },

  /**
   * A handler for the Add Site button.
   * @private
   */
  onAddSiteTap_() {
    assert(!this.readOnlyList);
    this.showAddSiteDialog_ = true;
  },

  /** @private */
  onAddSiteDialogClosed_() {
    this.showAddSiteDialog_ = false;
    focusWithoutInk(assert(this.$.addSite));
  },

  /**
   * Need to use common tooltip since the tooltip in the entry is cut off from
   * the iron-list.
   * @param {!CustomEvent<!{target: HTMLElement, text: string}>} e
   * @private
   */
  onShowTooltip_(e) {
    this.tooltipText_ = e.detail.text;
    const target = e.detail.target;
    // paper-tooltip normally determines the target from the |for| property,
    // which is a selector. Here paper-tooltip is being reused by multiple
    // potential targets.
    const tooltip = this.$.tooltip;
    tooltip.target = target;
    /** @type {{updatePosition: Function}} */ (tooltip).updatePosition();
    const hide = () => {
      this.$.tooltip.hide();
      target.removeEventListener('mouseleave', hide);
      target.removeEventListener('blur', hide);
      target.removeEventListener('click', hide);
      this.$.tooltip.removeEventListener('mouseenter', hide);
    };
    target.addEventListener('mouseleave', hide);
    target.addEventListener('blur', hide);
    target.addEventListener('click', hide);
    this.$.tooltip.addEventListener('mouseenter', hide);
    this.$.tooltip.show();
  },

  // <if expr="chromeos">
  /**
   * Load android sms info if required and sets it to the |androidSmsInfo_|
   * property. Returns a promise that resolves when load is complete.
   * @private
   */
  updateAndroidSmsInfo_() {
    // |androidSmsInfo_| is only relevant for NOTIFICATIONS category. Don't
    // bother fetching it for other categories.
    if (this.category === ContentSettingsTypes.NOTIFICATIONS &&
        loadTimeData.valueExists('multideviceAllowedByPolicy') &&
        loadTimeData.getBoolean('multideviceAllowedByPolicy') &&
        !this.androidSmsInfo_) {
      const androidInfoBrowserProxy = AndroidInfoBrowserProxyImpl.getInstance();
      return androidInfoBrowserProxy.getAndroidSmsInfo().then((info) => {
        this.androidSmsInfo_ = info;
      });
    }

    return Promise.resolve();
  },

  /**
   * Processes exceptions and adds showAndroidSmsNote field to
   * the required exception item.
   * @private
   */
  processExceptionsForAndroidSmsInfo_(sites) {
    if (!this.androidSmsInfo_ || !this.androidSmsInfo_.enabled) {
      return sites;
    }
    return sites.map((site) => {
      if (site.origin === this.androidSmsInfo_.origin) {
        return Object.assign({showAndroidSmsNote: true}, site);
      } else {
        return site;
      }
    });
  },
  // </if>

  /**
   * Populate the sites list for display.
   * @private
   */
  populateList_() {
    this.browserProxy_.getExceptionList(this.category).then(exceptionList => {
      this.processExceptions_(exceptionList);
      this.closeActionMenu_();
    });
  },

  /**
   * Process the exception list returned from the native layer.
   * @param {!Array<RawSiteException>} exceptionList
   * @private
   */
  processExceptions_(exceptionList) {
    let sites = exceptionList
                    .filter(
                        site => site.setting !== ContentSetting.DEFAULT &&
                            site.setting === this.categorySubtype)
                    .map(site => this.expandSiteException(site));

    // <if expr="chromeos">
    sites = this.processExceptionsForAndroidSmsInfo_(sites);
    // </if>
    this.updateList('sites', x => x.origin, sites);
  },

  /**
   * Set up the values to use for the action menu.
   * @private
   */
  setUpActionMenu_() {
    this.showAllowAction_ = this.categorySubtype !== ContentSetting.ALLOW;
    this.showBlockAction_ = this.categorySubtype !== ContentSetting.BLOCK;
    this.showSessionOnlyAction_ =
        this.categorySubtype !== ContentSetting.SESSION_ONLY &&
        this.category === ContentSettingsTypes.COOKIES;
  },

  /**
   * @return {boolean} Whether to show the "Session Only" menu item for the
   *     currently active site.
   * @private
   */
  showSessionOnlyActionForSite_() {
    // It makes no sense to show "clear on exit" for exceptions that only apply
    // to incognito. It gives the impression that they might under some
    // circumstances not be cleared on exit, which isn't true.
    if (!this.actionMenuSite_ || this.actionMenuSite_.incognito) {
      return false;
    }

    return this.showSessionOnlyAction_;
  },

  /**
   * @param {!ContentSetting} contentSetting
   * @private
   */
  setContentSettingForActionMenuSite_(contentSetting) {
    assert(this.actionMenuSite_);
    this.browserProxy.setCategoryPermissionForPattern(
        this.actionMenuSite_.origin, this.actionMenuSite_.embeddingOrigin,
        this.category, contentSetting, this.actionMenuSite_.incognito);
  },

  /** @private */
  onAllowTap_() {
    this.setContentSettingForActionMenuSite_(ContentSetting.ALLOW);
    this.closeActionMenu_();
  },

  /** @private */
  onBlockTap_() {
    this.setContentSettingForActionMenuSite_(ContentSetting.BLOCK);
    this.closeActionMenu_();
  },

  /** @private */
  onSessionOnlyTap_() {
    this.setContentSettingForActionMenuSite_(ContentSetting.SESSION_ONLY);
    this.closeActionMenu_();
  },

  /** @private */
  onEditTap_() {
    // Close action menu without resetting |this.actionMenuSite_| since it is
    // bound to the dialog.
    /** @type {!CrActionMenuElement} */ (this.$$('cr-action-menu')).close();
    this.showEditExceptionDialog_ = true;
  },

  /** @private */
  onEditExceptionDialogClosed_() {
    this.showEditExceptionDialog_ = false;
    this.actionMenuSite_ = null;
    if (this.activeDialogAnchor_) {
      this.activeDialogAnchor_.focus();
      this.activeDialogAnchor_ = null;
    }
  },

  /** @private */
  onResetTap_() {
    const site = this.actionMenuSite_;
    assert(site);
    this.browserProxy.resetCategoryPermissionForPattern(
        site.origin, site.embeddingOrigin, this.category, site.incognito);
    this.closeActionMenu_();
  },

  /**
   * @param {!Event} e
   * @private
   */
  onShowActionMenu_(e) {
    this.activeDialogAnchor_ = /** @type {!HTMLElement} */ (e.detail.anchor);
    this.actionMenuSite_ = e.detail.model;
    /** @type {!CrActionMenuElement} */ (this.$$('cr-action-menu'))
        .showAt(this.activeDialogAnchor_);
  },

  /** @private */
  closeActionMenu_() {
    this.actionMenuSite_ = null;
    this.activeDialogAnchor_ = null;
    const actionMenu =
        /** @type {!CrActionMenuElement} */ (this.$$('cr-action-menu'));
    if (actionMenu.open) {
      actionMenu.close();
    }
  },

  /**
   * @return {!Array<!SiteException>}
   * @private
   */
  getFilteredSites_() {
    if (!this.searchFilter) {
      return this.sites.slice();
    }

    const propNames = [
      'displayName',
      'origin',
    ];
    const searchFilter = this.searchFilter.toLowerCase();
    return this.sites.filter(
        site => propNames.some(
            propName => site[propName].toLowerCase().includes(searchFilter)));
  },

  /**
   * @return {string}
   * @private
   */
  getCssClass_() {
    return this.enableContentSettingsRedesign_ ? 'secondary' : '';
  }
});
