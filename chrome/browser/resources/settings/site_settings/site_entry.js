// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'site-entry' is an element representing a single eTLD+1 site entity.
 */
Polymer({
  is: 'site-entry',

  behaviors: [SiteSettingsBehavior, cr.ui.FocusRowBehavior],

  properties: {
    /**
     * An object representing a group of sites with the same eTLD+1.
     * @type {!SiteGroup}
     */
    siteGroup: {
      type: Object,
      observer: 'onSiteGroupChanged_',
    },

    /**
     * The name to display beside the icon. If grouped_() is true, it will be
     * the eTLD+1 for all the origins, otherwise, it will return the host.
     * @private
     */
    displayName_: String,

    /**
     * The string to display when there is a non-zero number of cookies.
     * @private
     */
    cookieString_: String,

    /**
     * The position of this site-entry in its parent list.
     */
    listIndex: {
      type: Number,
      value: -1,
    },

    /**
     * The string to display showing the overall usage of this site-entry.
     * @private
     */
    overallUsageString_: String,

    /**
     * An array containing the strings to display showing the individual disk
     * usage for each origin in |siteGroup|.
     * @type {!Array<string>}
     * @private
     */
    originUsages_: {
      type: Array,
      value: function() {
        return [];
      },
    },

    /**
     * An array containing the strings to display showing the individual cookies
     * number for each origin in |siteGroup|.
     * @type {!Array<string>}
     * @private
     */
    cookiesNum_: {
      type: Array,
      value: function() {
        return [];
      },
    },

    /**
     * The selected sort method.
     * @type {!settings.SortMethod|undefined}
     */
    sortMethod: {type: String, observer: 'updateOrigins_'}
  },

  /** @private {?settings.LocalDataBrowserProxy} */
  localDataBrowserProxy_: null,

  /** @private {?Element} */
  button_: null,

  /** @override */
  created: function() {
    this.localDataBrowserProxy_ =
        settings.LocalDataBrowserProxyImpl.getInstance();
  },

  /** @override */
  detached: function() {
    if (this.button_) {
      this.unlisten(this.button_, 'keydown', 'onButtonKeydown_');
    }
  },

  /** @param {!KeyboardEvent} e */
  onButtonKeydown_: function(e) {
    if (e.shiftKey && e.key === 'Tab') {
      this.focus();
    }
  },

  /**
   * Whether the list of origins displayed in this site-entry is a group of
   * eTLD+1 origins or not.
   * @param {SiteGroup} siteGroup The eTLD+1 group of origins.
   * @return {boolean}
   * @private
   */
  grouped_: function(siteGroup) {
    if (!siteGroup) {
      return false;
    }
    if (siteGroup.origins.length > 1 ||
        siteGroup.numCookies > siteGroup.origins[0].numCookies) {
      return true;
    }
    return false;
  },

  /**
   * Returns a user-friendly name for the siteGroup.
   * If grouped_() is true and eTLD+1 is available, returns the eTLD+1,
   * otherwise return the origin representation for the first origin.
   * @param {SiteGroup} siteGroup The eTLD+1 group of origins.
   * @return {string} The user-friendly name.
   * @private
   */
  siteGroupRepresentation_: function(siteGroup) {
    if (!siteGroup) {
      return '';
    }
    if (this.grouped_(siteGroup)) {
      if (siteGroup.etldPlus1 != '') {
        return siteGroup.etldPlus1;
      }
      // Fall back onto using the host of the first origin, if no eTLD+1 name
      // was computed.
    }
    return this.originRepresentation_(siteGroup.origins[0]);
  },

  /**
   * Returns a user-friendly name for the origin.
   * @param {OriginInfo} origin
   * @return {string} The user-friendly name.
   * @private
   */
  originRepresentation_(origin) {
    const url = this.toUrl(origin.origin);
    return url.host;
  },

  /**
   * @param {SiteGroup} siteGroup The eTLD+1 group of origins.
   * @private
   */
  onSiteGroupChanged_: function(siteGroup) {
    // Update the button listener.
    if (this.button_) {
      this.unlisten(this.button_, 'keydown', 'onButtonKeydown_');
    }
    this.button_ = /** @type Element */
        (this.root.querySelector('#toggleButton *:not([hidden])'));
    this.listen(assert(this.button_), 'keydown', 'onButtonKeydown_');

    if (!this.grouped_(siteGroup)) {
      // Ensure ungrouped |siteGroup|s do not get stuck in an opened state.
      const collapseChild = this.$.originList.getIfExists();
      if (collapseChild && collapseChild.opened) {
        this.toggleCollapsible_();
      }
    }
    if (!siteGroup) {
      return;
    }
    this.calculateUsageInfo_(siteGroup);
    this.getCookieNumString_(siteGroup.numCookies).then(string => {
      this.cookieString_ = string;
    });
    this.updateOrigins_(this.sortMethod);
    this.displayName_ = this.siteGroupRepresentation_(siteGroup);
  },

  /**
   * Returns any non-HTTPS scheme/protocol for the siteGroup that only contains
   * one origin. Otherwise, returns a empty string.
   * @param {SiteGroup} siteGroup The eTLD+1 group of origins.
   * @return {string} The scheme if non-HTTPS, or empty string if HTTPS.
   * @private
   */
  siteGroupScheme_: function(siteGroup) {
    if (!siteGroup || (this.grouped_(siteGroup))) {
      return '';
    }
    return this.originScheme_(siteGroup.origins[0]);
  },

  /**
   * Returns any non-HTTPS scheme/protocol for the origin. Otherwise, returns
   * an empty string.
   * @param {OriginInfo} origin
   * @return {string} The scheme if non-HTTPS, or empty string if HTTPS.
   * @private
   */
  originScheme_: function(origin) {
    const url = this.toUrl(origin.origin);
    const scheme = url.protocol.replace(new RegExp(':*$'), '');
    /** @type{string} */ const HTTPS_SCHEME = 'https';
    if (scheme == HTTPS_SCHEME) {
      return '';
    }
    return scheme;
  },

  /**
   * Get an appropriate favicon that represents this group of eTLD+1 sites as a
   * whole.
   * @param {!SiteGroup} siteGroup The eTLD+1 group of origins.
   * @return {string} URL that is used for fetching the favicon
   * @private
   */
  getSiteGroupIcon_: function(siteGroup) {
    const origins = siteGroup.origins;
    assert(origins);
    assert(origins.length >= 1);
    if (origins.length == 1) {
      return origins[0].origin;
    }
    // If we can find a origin with format "www.etld+1", use the favicon of this
    // origin. Otherwise find the origin with largest storage, and use the
    // number of cookies as a tie breaker.
    for (const originInfo of origins) {
      if (this.toUrl(originInfo.origin).host == 'www.' + siteGroup.etldPlus1) {
        return originInfo.origin;
      }
    }
    const getMaxStorage = (max, originInfo) => {
      return (
          max.usage > originInfo.usage ||
                  (max.usage == originInfo.usage &&
                   max.numCookies > originInfo.numCookies) ?
              max :
              originInfo);
    };
    return origins.reduce(getMaxStorage, origins[0]).origin;
  },

  /**
   * Calculates the amount of disk storage used by the given eTLD+1.
   * Also updates the corresponding display strings.
   * @param {SiteGroup} siteGroup The eTLD+1 group of origins.
   * @private
   */
  calculateUsageInfo_: function(siteGroup) {
    let overallUsage = 0;
    this.siteGroup.origins.forEach((originInfo, i) => {
      overallUsage += originInfo.usage;
    });
    this.browserProxy.getFormattedBytes(overallUsage).then(string => {
      this.overallUsageString_ = string;
    });
  },

  /**
   * Get display string for number of cookies.
   * @param {number} numCookies
   * @private
   */
  getCookieNumString_: function(numCookies) {
    if (numCookies == 0) {
      return Promise.resolve('');
    }
    return this.localDataBrowserProxy_.getNumCookiesString(numCookies);
  },

  /**
   * Array binding for the |originUsages_| array for use in the HTML.
   * @param {!{base: !Array<string>}} change The change record for the array.
   * @param {number} index The index of the array item.
   * @return {string}
   * @private
   */
  originUsagesItem_: function(change, index) {
    return change.base[index];
  },

  /**
   * Array binding for the |cookiesNum_| array for use in the HTML.
   * @param {!{base: !Array<string>}} change The change record for the array.
   * @param {number} index The index of the array item.
   * @return {string}
   * @private
   */
  originCookiesItem_: function(change, index) {
    return change.base[index];
  },

  /**
   * Navigates to the corresponding Site Details page for the given origin.
   * @param {string} origin The origin to navigate to the Site Details page for
   * it.
   * @private
   */
  navigateToSiteDetails_: function(origin) {
    this.fire(
        'site-entry-selected', {item: this.siteGroup, index: this.listIndex});
    settings.navigateTo(
        settings.routes.SITE_SETTINGS_SITE_DETAILS,
        new URLSearchParams('site=' + origin));
  },

  /**
   * A handler for selecting a site (by clicking on the origin).
   * @param {!{model: !{index: !number}}} e
   * @private
   */
  onOriginTap_: function(e) {
    this.navigateToSiteDetails_(this.siteGroup.origins[e.model.index].origin);
    this.browserProxy.recordAction(settings.AllSitesAction.ENTER_SITE_DETAILS);
  },

  /**
   * A handler for clicking on a site-entry heading. This will either show a
   * list of origins or directly navigates to Site Details if there is only one.
   * @private
   */
  onSiteEntryTap_: function() {
    // Individual origins don't expand - just go straight to Site Details.
    if (!this.grouped_(this.siteGroup)) {
      this.navigateToSiteDetails_(this.siteGroup.origins[0].origin);
      this.browserProxy.recordAction(
          settings.AllSitesAction.ENTER_SITE_DETAILS);
      return;
    }
    this.toggleCollapsible_();

    // Make sure the expanded origins can be viewed without further scrolling
    // (in case |this| is already at the bottom of the viewport).
    this.scrollIntoViewIfNeeded();
  },

  /**
   * Toggles open and closed the list of origins if there is more than one.
   * @private
   */
  toggleCollapsible_: function() {
    const collapseChild =
        /** @type {IronCollapseElement} */ (this.$.originList.get());
    collapseChild.toggle();
    this.$.toggleButton.setAttribute('aria-expanded', collapseChild.opened);
    this.$.expandIcon.toggleClass('icon-expand-more');
    this.$.expandIcon.toggleClass('icon-expand-less');
    this.fire('iron-resize');
  },

  /**
   * Fires a custom event when the menu button is clicked. Sends the details
   * of the site entry item and where the menu should appear.
   * @param {!Event} e
   * @private
   */
  showOverflowMenu_: function(e) {
    this.fire('open-menu', {
      target: e.target,
      index: this.listIndex,
      item: this.siteGroup,
    });
  },

  /**
   * Returns a valid index for an origin contained in |siteGroup.origins| by
   * clamping the given |index|. This also replaces undefined |index|es with 0.
   * Use this to prevent being given out-of-bounds indexes by dom-repeat when
   * scrolling an iron-list storing these site-entries too quickly.
   * @param {!number=} index
   * @return {number}
   * @private
   */
  getIndexBoundToOriginList_: function(siteGroup, index) {
    return Math.max(0, Math.min(index, siteGroup.origins.length - 1));
  },

  /**
   * Returns the correct class to apply depending on this site-entry's position
   * in a list.
   * @param {number} index
   * @private
   */
  getClassForIndex_: function(index) {
    if (index == 0) {
      return 'first';
    }
    return '';
  },

  /**
   * Update the order and data display text for origins.
   * @param {!settings.SortMethod|undefined} sortMethod
   * @private
   */
  updateOrigins_: function(sortMethod) {
    if (!sortMethod || !this.siteGroup || !this.grouped_(this.siteGroup)) {
      return null;
    }

    const origins = this.siteGroup.origins.slice();
    origins.sort(this.sortFunction_(sortMethod));
    this.set('siteGroup.origins', origins);

    this.originUsages_ = new Array(origins.length);
    origins.forEach((originInfo, i) => {
      this.browserProxy.getFormattedBytes(originInfo.usage).then((string) => {
        this.set(`originUsages_.${i}`, string);
      });
    });

    this.cookiesNum_ = new Array(this.siteGroup.origins.length);
    origins.forEach((originInfo, i) => {
      this.getCookieNumString_(originInfo.numCookies).then((string) => {
        this.set(`cookiesNum_.${i}`, string);
      });
    });
  },

  /**
   * Sort functions for sorting origins based on selected method.
   * @param {!settings.SortMethod|undefined} sortMethod
   * @private
   */
  sortFunction_: function(sortMethod) {
    if (sortMethod == settings.SortMethod.MOST_VISITED) {
      return (origin1, origin2) => {
        return origin2.engagement - origin1.engagement;
      };
    } else if (sortMethod == settings.SortMethod.STORAGE) {
      return (origin1, origin2) => {
        return origin2.usage - origin1.usage ||
            origin2.numCookies - origin1.numCookies;
      };
    } else if (sortMethod == settings.SortMethod.NAME) {
      return (origin1, origin2) => {
        return origin1.origin.localeCompare(origin2.origin);
      };
    }
  },
});
