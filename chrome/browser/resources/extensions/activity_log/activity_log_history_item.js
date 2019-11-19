// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import '../shared_style.js';
import '../shared_vars.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @typedef {{
 *   activityIds: !Set<string>,
 *   key: string,
 *   count: number,
 *   activityType: !chrome.activityLogPrivate.ExtensionActivityFilter,
 *   countsByUrl: !Map<string, number>,
 *   expanded: boolean
 * }}
 */
export let ActivityGroup;

/**
 * A struct used to describe each url and its associated counts. The id is
 * unique for each item in the list of URLs and is used for the tooltip.
 * @typedef {{
 *   page: string,
 *   count: number
 * }}
 */
let PageUrlItem;

Polymer({
  is: 'activity-log-history-item',

  _template: html`{__html_template__}`,

  properties: {
    /**
     * The underlying ActivityGroup that provides data for the
     * ActivityLogItem displayed.
     * @type {!ActivityGroup}
     */
    data: Object,

    /** @private */
    isExpandable_: {
      type: Boolean,
      computed: 'computeIsExpandable_(data.countsByUrl)',
    },
  },

  /**
   * @private
   * @return {boolean}
   */
  computeIsExpandable_: function() {
    return this.data.countsByUrl.size > 0;
  },

  /**
   * Sort the page URLs by the number of times it was associated with the key
   * for this ActivityGroup (API call or content script invocation.) Resolve
   * ties by the alphabetical order of the page URL.
   * @private
   * @return {!Array<PageUrlItem>}
   */
  getPageUrls_: function() {
    return Array.from(this.data.countsByUrl.entries())
        .map(e => ({page: e[0], count: e[1]}))
        .sort(function(a, b) {
          if (a.count != b.count) {
            return b.count - a.count;
          }
          return a.page < b.page ? -1 : (a.page > b.page ? 1 : 0);
        });
  },

  /** @private */
  onDeleteTap_: function(e) {
    e.stopPropagation();
    this.fire(
        'delete-activity-log-item', Array.from(this.data.activityIds.values()));
  },

  /** @private */
  onExpandTap_: function() {
    if (this.isExpandable_) {
      this.set('data.expanded', !this.data.expanded);
    }
  },

  /**
   * Show the call count for a particular page URL if more than one page
   * URL is associated with the key for this ActivityGroup.
   * @private
   * @return {boolean}
   */
  shouldShowPageUrlCount_: function() {
    return this.data.countsByUrl.size > 1;
  },
});
