// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_search_field/cr_search_field.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import '../shared_style.css.js';
import './activity_log_stream_item.js';

import type {ChromeEvent} from '/tools/typescript/definitions/chrome_event.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './activity_log_stream.html.js';
import type {StreamItem} from './activity_log_stream_item.js';

export interface ActivityLogEventDelegate {
  getOnExtensionActivity(): ChromeEvent<
      (activity: chrome.activityLogPrivate.ExtensionActivity) => void>;
}

/**
 * Process activity for the stream. In the case of content scripts, we split
 * the activity for every script invoked.
 */
function processActivityForStream(
    activity: chrome.activityLogPrivate.ExtensionActivity): StreamItem[] {
  const activityType = activity.activityType;
  const timestamp = activity.time!;
  const isContentScript = activityType ===
      chrome.activityLogPrivate.ExtensionActivityType.CONTENT_SCRIPT;

  const args = isContentScript ? JSON.stringify([]) : activity.args!;

  let streamItemNames = [activity.apiCall!];

  // TODO(kelvinjiang): Reuse logic from activity_log_history and refactor
  // some of the processing code into a separate file in a follow up CL.
  if (isContentScript) {
    streamItemNames = activity.args ? JSON.parse(activity.args) : [];
    assert(Array.isArray(streamItemNames), 'Invalid data for script names.');
  }

  const other = activity.other;
  const webRequestInfo = other && other.webRequest;

  return streamItemNames.map(name => ({
                               args,
                               argUrl: activity.argUrl!,
                               activityType,
                               name,
                               pageUrl: activity.pageUrl,
                               timestamp,
                               webRequestInfo,
                               expanded: false,
                             }));
}

export class ActivityLogStreamElement extends PolymerElement {
  static get is() {
    return 'activity-log-stream';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      extensionId: String,
      delegate: Object,

      isStreamOn_: {
        type: Boolean,
        value: false,
      },

      activityStream_: {
        type: Array,
        value: () => [],
      },

      filteredActivityStream_: {
        type: Array,
        computed:
            'computeFilteredActivityStream_(activityStream_.*, lastSearch_)',
      },

      lastSearch_: {
        type: String,
        value: '',
      },
    };
  }

  extensionId: string;
  delegate: ActivityLogEventDelegate;
  private isStreamOn_: boolean;
  private activityStream_: StreamItem[];
  private filteredActivityStream_: StreamItem[];
  private lastSearch_: string;
  private listenerInstance_:
      (type: chrome.activityLogPrivate.ExtensionActivity) => void;

  constructor() {
    super();

    /**
     * Instance of |extensionActivityListener_| bound to |this|.
     */
    this.listenerInstance_ = () => {};
  }

  override connectedCallback() {
    super.connectedCallback();

    // Since this component is not restamped, this will only be called once
    // in its lifecycle.
    this.listenerInstance_ = this.extensionActivityListener_.bind(this);
    this.startStream();
  }

  private onResizeStream_() {
    this.shadowRoot!.querySelector('iron-list')!.notifyResize();
  }

  clearStream() {
    this.splice('activityStream_', 0, this.activityStream_.length);
  }

  startStream() {
    if (this.isStreamOn_) {
      return;
    }

    this.isStreamOn_ = true;
    this.delegate.getOnExtensionActivity().addListener(this.listenerInstance_);
  }

  pauseStream() {
    if (!this.isStreamOn_) {
      return;
    }

    this.delegate.getOnExtensionActivity().removeListener(
        this.listenerInstance_);
    this.isStreamOn_ = false;
  }

  private onToggleButtonClick_() {
    if (this.isStreamOn_) {
      this.pauseStream();
    } else {
      this.startStream();
    }
  }

  private isStreamEmpty_(): boolean {
    return this.activityStream_.length === 0;
  }

  private isFilteredStreamEmpty_(): boolean {
    return this.filteredActivityStream_.length === 0;
  }

  private shouldShowEmptySearchMessage_(): boolean {
    return !this.isStreamEmpty_() && this.isFilteredStreamEmpty_();
  }

  private extensionActivityListener_(
      activity: chrome.activityLogPrivate.ExtensionActivity) {
    if (activity.extensionId !== this.extensionId) {
      return;
    }

    this.splice(
        'activityStream_', this.activityStream_.length, 0,
        ...processActivityForStream(activity));

    // Used to update the scrollbar.
    this.shadowRoot!.querySelector('iron-list')!.notifyResize();
  }

  private onSearchChanged_(e: CustomEvent<string>) {
    // Remove all whitespaces from the search term, as API call names and
    // URLs should not contain any whitespace. As of now, only single term
    // search queries are allowed.
    const searchTerm = e.detail.replace(/\s+/g, '').toLowerCase();
    if (searchTerm === this.lastSearch_) {
      return;
    }

    this.lastSearch_ = searchTerm;
  }

  private computeFilteredActivityStream_(): StreamItem[] {
    if (!this.lastSearch_) {
      return this.activityStream_.slice();
    }

    // Match on these properties for each activity.
    const propNames = [
      'name',
      'pageUrl',
      'activityType',
    ];

    return this.activityStream_.filter(act => {
      return propNames.some(prop => {
        const value = (act as {[index: string]: any})[prop];
        return value && value.toLowerCase().includes(this.lastSearch_);
      });
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'activity-log-stream': ActivityLogStreamElement;
  }
}

customElements.define(ActivityLogStreamElement.is, ActivityLogStreamElement);
