// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './activity_log_history_item.css.js';
import {getHtml} from './activity_log_history_item.html.js';

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

export class ActivityLogHistoryItemElement extends CrLitElement {
  static get is() {
    return 'activity-log-history-item';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /**
       * The underlying ActivityGroup that provides data for the
       * ActivityLogItem displayed.
       */
      data: {type: Object},

      expanded_: {type: Boolean},
      isExpandable_: {type: Boolean},
    };
  }

  data: ActivityGroup = {
    activityIds: new Set<string>(),
    key: '',
    count: 0,
    activityType: chrome.activityLogPrivate.ExtensionActivityFilter.API_CALL,
    countsByUrl: new Map<string, number>(),
    expanded: false,
  };
  protected expanded_: boolean = false;
  protected isExpandable_: boolean = false;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('data')) {
      this.isExpandable_ = this.data.countsByUrl.size > 0;
      this.expanded_ = this.data.expanded;
    }
  }

  expand(expanded: boolean) {
    if (this.isExpandable_) {
      this.expanded_ = expanded;
    }
  }

  /**
   * Sort the page URLs by the number of times it was associated with the key
   * for this ActivityGroup (API call or content script invocation.) Resolve
   * ties by the alphabetical order of the page URL.
   */
  protected getPageUrls_(): PageUrlItem[] {
    return Array.from(this.data.countsByUrl.entries())
        .map(e => ({page: e[0], count: e[1]}))
        .sort(function(a, b) {
          if (a.count !== b.count) {
            return b.count - a.count;
          }
          return a.page < b.page ? -1 : (a.page > b.page ? 1 : 0);
        });
  }

  protected onDeleteClick_(e: Event) {
    e.stopPropagation();
    this.fire(
        'delete-activity-log-item', Array.from(this.data.activityIds.values()));
  }

  protected onExpandClick_() {
    if (this.isExpandable_) {
      this.expanded_ = !this.expanded_;
    }
  }

  protected onExpandedChanged_(e: CustomEvent<{value: boolean}>) {
    this.expanded_ = e.detail.value;
  }

  /**
   * Show the call count for a particular page URL if more than one page
   * URL is associated with the key for this ActivityGroup.
   */
  protected shouldShowPageUrlCount_(): boolean {
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
