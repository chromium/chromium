// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'site-list' shows a list of Allowed and Blocked sites for a given
 * category.
 */
Polymer({
  is: 'site-list',

  behaviors: [
    SiteSettingsBehavior,
    WebUIListenerBehavior,
    ListPropertyUpdateBehavior,
  ],

  properties: {
    /**
     * Some content types (like Location) do not allow the user to manually
     * edit the exception list from within Settings.
     * @private
     */
    readOnlyList: {
      type: Boolean,
      value: false,
    },

    categoryHeader: String,

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
      value: function() {
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
      value: settings.INVALID_CATEGORY_SUBTYPE,
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
   * @private {?settings.AndroidSmsInfo}
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
  ready: function() {
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
  siteWithinCategoryChanged_: function(category, site) {
    if (category == this.category) {
      this.configureWidget_();
    }
  },

  /**
   * Called for each site list when incognito is enabled or disabled. Only
   * called on change (opening N incognito windows only fires one message).
   * Another message is sent when the *last* incognito window closes.
   * @private
   */
  onIncognitoStatusChanged_: function(hasIncognito) {
    this.hasIncognito_ = hasIncognito;

    // The SESSION_ONLY list won't have any incognito exceptions. (Minor
    // optimization, not required).
    if (this.categorySubtype == settings.ContentSetting.SESSION_ONLY) {
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
  configureWidget_: function() {
    if (this.category == undefined) {
      return;
    }

    // The observer for All Sites fires before the attached/ready event, so
    // initialize this here.
    if (this.browserProxy_ === undefined) {
      this.browserProxy_ =
          settings.SiteSettingsPrefsBrowserProxyImpl.getInstance();
    }

    this.setUpActionMenu_();

    // <if expr="not chromeos">
    this.populateList_();
    // </if>

    // <if expr="chromeos">
    this.updateAndroidSmsInfo_().then(this.populateList_.bind(this));
    // </if>

    // The Session permissions are only for cookies.
    if (this.categorySubtype == settings.ContentSetting.SESSION_ONLY) {
      this.$.category.hidden =
          this.category != settings.ContentSettingsTypes.COOKIES;
    }
  },

  /**
   * Whether there are any site exceptions added for this content setting.
   * @return {boolean}
   * @private
   */
  hasSites_: function() {
    return this.sites.length > 0;
  },

  /**
   * Whether the Add Site button is shown in the header for the current category
   * and category subtype.
   * @return {boolean}
   * @private
   */
  computeShowAddSiteButton_: function() {
    return !(
        this.readOnlyList ||
        (this.category ==
             settings.ContentSettingsTypes.NATIVE_FILE_SYSTEM_WRITE &&
         this.categorySubtype == settings.ContentSetting.ALLOW));
  },

  /**
   * @return {boolean}
   * @private
   */
  showNoSearchResults_: function() {
    return this.sites.length > 0 && this.getFilteredSites_().length == 0;
  },

  /**
   * A handler for the Add Site button.
   * @private
   */
  onAddSiteTap_: function() {
    assert(!this.readOnlyList);
    this.showAddSiteDialog_ = true;
  },

  /** @private */
  onAddSiteDialogClosed_: function() {
    this.showAddSiteDialog_ = false;
    cr.ui.focusWithoutInk(assert(this.$.addSite));
  },

  /**
   * Need to use common tooltip since the tooltip in the entry is cut off from
   * the iron-list.
   * @param {!CustomEvent<!{target: HTMLElement, text: string}>} e
   * @private
   */
  onShowTooltip_: function(e) {
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
      target.removeEventListener('tap', hide);
      this.$.tooltip.removeEventListener('mouseenter', hide);
    };
    target.addEventListener('mouseleave', hide);
    target.addEventListener('blur', hide);
    target.addEventListener('tap', hide);
    this.$.tooltip.addEventListener('mouseenter', hide);
    this.$.tooltip.show();
  },

  // <if expr="chromeos">
  /**
   * Load android sms info if required and sets it to the |androidSmsInfo_|
   * property. Returns a promise that resolves when load is complete.
   * @private
   */
  updateAndroidSmsInfo_: function() {
    // |androidSmsInfo_| is only relevant for NOTIFICATIONS category. Don't
    // bother fetching it for other categories.
    if (this.category === settings.ContentSettingsTypes.NOTIFICATIONS &&
        loadTimeData.valueExists('multideviceAllowedByPolicy') &&
        loadTimeData.getBoolean('multideviceAllowedByPolicy') &&
        !this.androidSmsInfo_) {
      const multideviceSetupProxy =
          settings.MultiDeviceBrowserProxyImpl.getInstance();
      return multideviceSetupProxy.getAndroidSmsInfo().then((info) => {
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
  processExceptionsForAndroidSmsInfo_: function(sites) {
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
  populateList_: function() {
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
  processExceptions_: function(exceptionList) {
    let sites =
        exceptionList
            .filter(
                site => site.setting != settings.ContentSetting.DEFAULT &&
                    site.setting == this.categorySubtype)
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
  setUpActionMenu_: function() {
    this.showAllowAction_ =
        this.categorySubtype != settings.ContentSetting.ALLOW;
    this.showBlockAction_ =
        this.categorySubtype != settings.ContentSetting.BLOCK;
    this.showSessionOnlyAction_ =
        this.categorySubtype != settings.ContentSetting.SESSION_ONLY &&
        this.category == settings.ContentSettingsTypes.COOKIES;
  },

  /**
   * @return {boolean} Whether to show the "Session Only" menu item for the
   *     currently active site.
   * @private
   */
  showSessionOnlyActionForSite_: function() {
    // It makes no sense to show "clear on exit" for exceptions that only apply
    // to incognito. It gives the impression that they might under some
    // circumstances not be cleared on exit, which isn't true.
    if (!this.actionMenuSite_ || this.actionMenuSite_.incognito) {
      return false;
    }

    return this.showSessionOnlyAction_;
  },

  /**
   * @param {!settings.ContentSetting} contentSetting
   * @private
   */
  setContentSettingForActionMenuSite_: function(contentSetting) {
    assert(this.actionMenuSite_);
    this.browserProxy.setCategoryPermissionForPattern(
        this.actionMenuSite_.origin, this.actionMenuSite_.embeddingOrigin,
        this.category, contentSetting, this.actionMenuSite_.incognito);
  },

  /** @private */
  onAllowTap_: function() {
    this.setContentSettingForActionMenuSite_(settings.ContentSetting.ALLOW);
    this.closeActionMenu_();
  },

  /** @private */
  onBlockTap_: function() {
    this.setContentSettingForActionMenuSite_(settings.ContentSetting.BLOCK);
    this.closeActionMenu_();
  },

  /** @private */
  onSessionOnlyTap_: function() {
    this.setContentSettingForActionMenuSite_(
        settings.ContentSetting.SESSION_ONLY);
    this.closeActionMenu_();
  },

  /** @private */
  onEditTap_: function() {
    // Close action menu without resetting |this.actionMenuSite_| since it is
    // bound to the dialog.
    /** @type {!CrActionMenuElement} */ (this.$$('cr-action-menu')).close();
    this.showEditExceptionDialog_ = true;
  },

  /** @private */
  onEditExceptionDialogClosed_: function() {
    this.showEditExceptionDialog_ = false;
    this.actionMenuSite_ = null;
    if (this.activeDialogAnchor_) {
      this.activeDialogAnchor_.focus();
      this.activeDialogAnchor_ = null;
    }
  },

  /** @private */
  onResetTap_: function() {
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
  onShowActionMenu_: function(e) {
    this.activeDialogAnchor_ = /** @type {!HTMLElement} */ (e.detail.anchor);
    this.actionMenuSite_ = e.detail.model;
    /** @type {!CrActionMenuElement} */ (this.$$('cr-action-menu'))
        .showAt(this.activeDialogAnchor_);
  },

  /** @private */
  closeActionMenu_: function() {
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
  getFilteredSites_: function() {
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
});
