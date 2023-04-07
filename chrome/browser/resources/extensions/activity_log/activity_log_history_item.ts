// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import '../shared_style.css.js';
import '../shared_vars.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './activity_log_history_item.html.js';

export interface ActivityGroup {
  activityIds: Set<string>;
  key: string;
  count: number;
  activityType: chrome.activityLogPrivate.ExtensionActivityFilter;
  countsByUrl: Map<string, number>;
  expanded: boolean;
}

/**
 * A struct used to describe each url and its associated counts. The id is
 * unique for each item in the list of URLs and is used for the tooltip.
 */
export interface PageUrlItem {
  page: string;
  count: number;
}

export class ActivityLogHistoryItemElement extends PolymerElement {
  static get is() {
    return 'activity-log-history-item';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The underlying ActivityGroup that provides data for the
       * ActivityLogItem displayed.
       */
      data: Object,

      isExpandable_: {
        type: Boolean,
        computed: 'computeIsExpandable_(data.countsByUrl)',
      },
    };
  }

  data: ActivityGroup;
  private isExpandable_: boolean;

  private computeIsExpandable_(): boolean {
    return this.data.countsByUrl.size > 0;
  }

  /**
   * Sort the page URLs by the number of times it was associated with the key
   * for this ActivityGroup (API call or content script invocation.) Resolve
   * ties by the alphabetical order of the page URL.
   */
  private getPageUrls_(): PageUrlItem[] {
    return Array.from(this.data.countsByUrl.entries())
        .map(e => ({page: e[0], count: e[1]}))
        .sort(function(a, b) {
          if (a.count !== b.count) {
            return b.count - a.count;
          }
          return a.page < b.page ? -1 : (a.page > b.page ? 1 : 0);
        });
  }

  private onDeleteClick_(e: Event) {
    e.stopPropagation();
    this.dispatchEvent(new CustomEvent('delete-activity-log-item', {
      bubbles: true,
      composed: true,
      detail: Array.from(this.data.activityIds.values()),
    }));
  }

  private onExpandClick_() {
    if (this.isExpandable_) {
      this.set('data.expanded', !this.data.expanded);
    }
  }

  /**
   * Show the call count for a particular page URL if more than one page
   * URL is associated with the key for this ActivityGroup.
   */
  private shouldShowPageUrlCount_(): boolean {
    return this.data.countsByUrl.size > 1;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'activity-log-history-item': ActivityLogHistoryItemElement;
  }
}

customElements.define(
    ActivityLogHistoryItemElement.is, ActivityLogHistoryItemElement);
