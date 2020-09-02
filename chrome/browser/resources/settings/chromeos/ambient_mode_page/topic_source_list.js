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
  },

  /**
   * @param {!AmbientModeTopicSource} topic_source
   * @private
   */
  isSelected_(topic_source) {
    return this.selectedTopicSource === topic_source;
  },
});
