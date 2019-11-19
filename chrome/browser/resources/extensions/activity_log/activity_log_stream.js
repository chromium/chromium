// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_search_field/cr_search_field.m.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import '../shared_style.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {StreamArgItem, StreamItem} from './activity_log_stream_item.js';


/** @interface */
export class ActivityLogEventDelegate {
  /** @return {!ChromeEvent} */
  getOnExtensionActivity() {}
}

/**
 * Process activity for the stream. In the case of content scripts, we split
 * the activity for every script invoked.
 * @param {!chrome.activityLogPrivate.ExtensionActivity}
 *     activity
 * @return {!Array<!StreamItem>}
 */
function processActivityForStream(activity) {
  const activityType = activity.activityType;
  const timestamp = activity.time;
  const isContentScript = activityType ===
      chrome.activityLogPrivate.ExtensionActivityType.CONTENT_SCRIPT;

  const args = isContentScript ? JSON.stringify([]) : activity.args;

  let streamItemNames = [activity.apiCall];

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
                               argUrl: activity.argUrl,
                               activityType,
                               name,
                               pageUrl: activity.pageUrl,
                               timestamp,
                               webRequestInfo,
                               expanded: false,
                             }));
}

Polymer({
  is: 'activity-log-stream',

  _template: html`{__html_template__}`,

  properties: {
    /** @type {string} */
    extensionId: String,

    /** @type {!ActivityLogEventDelegate} */
    delegate: Object,

    /** @private */
    isStreamOn_: {
      type: Boolean,
      value: false,
    },

    /** @private {!Array<!StreamItem>} */
    activityStream_: {
      type: Array,
      value: () => [],
    },

    /** @private {!Array<!StreamItem>} */
    filteredActivityStream_: {
      type: Array,
      computed:
          'computeFilteredActivityStream_(activityStream_.*, lastSearch_)',
    },

    /** @private */
    lastSearch_: {
      type: String,
      value: '',
    },
  },

  listeners: {
    'resize-stream': 'onResizeStream_',
  },

  /**
   * Instance of |extensionActivityListener_| bound to |this|.
   * @private {!Function}
   */
  listenerInstance_: () => {},

  /** @override */
  attached: function() {
    // Since this component is not restamped, this will only be called once
    // in its lifecycle.
    this.listenerInstance_ = this.extensionActivityListener_.bind(this);
    this.startStream();
  },

  /** @private */
  onResizeStream_: function(e) {
    this.$$('iron-list').notifyResize();
  },

  clearStream: function() {
    this.splice('activityStream_', 0, this.activityStream_.length);
  },

  startStream: function() {
    if (this.isStreamOn_) {
      return;
    }

    this.isStreamOn_ = true;
    this.delegate.getOnExtensionActivity().addListener(this.listenerInstance_);
  },

  pauseStream: function() {
    if (!this.isStreamOn_) {
      return;
    }

    this.delegate.getOnExtensionActivity().removeListener(
        this.listenerInstance_);
    this.isStreamOn_ = false;
  },

  /** @private */
  onToggleButtonClick_: function() {
    if (this.isStreamOn_) {
      this.pauseStream();
    } else {
      this.startStream();
    }
  },

  /**
   * @private
   * @return {boolean}
   */
  isStreamEmpty_: function() {
    return this.activityStream_.length == 0;
  },

  /**
   * @private
   * @return {boolean}
   */
  isFilteredStreamEmpty_: function() {
    return this.filteredActivityStream_.length == 0;
  },

  /**
   * @private
   * @return {boolean}
   */
  shouldShowEmptySearchMessage_: function() {
    return !this.isStreamEmpty_() && this.isFilteredStreamEmpty_();
  },

  /**
   * @private
   * @param {!chrome.activityLogPrivate.ExtensionActivity} activity
   */
  extensionActivityListener_: function(activity) {
    if (activity.extensionId != this.extensionId) {
      return;
    }

    this.splice(
        'activityStream_', this.activityStream_.length, 0,
        ...processActivityForStream(activity));

    // Used to update the scrollbar.
    this.$$('iron-list').notifyResize();
  },

  /**
   * @private
   * @param {!CustomEvent<string>} e
   */
  onSearchChanged_: function(e) {
    // Remove all whitespaces from the search term, as API call names and
    // URLs should not contain any whitespace. As of now, only single term
    // search queries are allowed.
    const searchTerm = e.detail.replace(/\s+/g, '').toLowerCase();
    if (searchTerm === this.lastSearch_) {
      return;
    }

    this.lastSearch_ = searchTerm;
  },

  /**
   * @private
   * @return {!Array<!StreamItem>}
   */
  computeFilteredActivityStream_: function() {
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
        return act[prop] && act[prop].toLowerCase().includes(this.lastSearch_);
      });
    });
  },
});
