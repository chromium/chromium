// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying a list of
 * AmbientModeTopicSource.
 */

/**
 * Polymer class definition for 'topic-source-list'.
 */
Polymer({
  is: 'topic-source-list',

  properties: {
    /**
     * Contains topic sources.
     * @type {!Array<!AmbientModeTopicSource>}
     */
    topicSources: {
      type: Array,
      value: [],
    },

    /** @type {!AmbientModeTopicSource} */
    selectedTopicSource: {
      type: AmbientModeTopicSource,
      value: AmbientModeTopicSource.UNKNOWN,
    },

    hasGooglePhotosAlbums: Boolean,

    /**
     * The items in this list will be disabled when |selectedTopicSource| is
     * |AmbientModeTopicSource.UNKNOWN|.
     */
    disabled: Boolean,
  },

  /**
   * @param {!AmbientModeTopicSource} topic_source
   * @private
   */
  isSelected_(topic_source) {
    return this.selectedTopicSource === topic_source;
  },

  /**
   * @param {number} tabIndex
   * @param {boolean} disabled
   * @return {number}
   * @private
   */
  computeTabIndex_(tabIndex, disabled) {
    // Disabled "topic-source-item" cannot be navigated into.
    if (disabled) {
      return -1;
    }
    return tabIndex;
  }
});
