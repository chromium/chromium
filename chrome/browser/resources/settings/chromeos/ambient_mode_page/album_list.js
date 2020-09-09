// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying a list of photo previews.
 */

/**
 * Polymer class definition for 'album-list'.
 */
Polymer({
  is: 'album-list',

  properties: {
    /** @private {!AmbientModeTopicSource} */
    topicSource: {
      type: Number,
      value() {
        return AmbientModeTopicSource.UNKNOWN;
      },
    },

    /** @private {?Array<!AmbientModeAlbum>} */
    albums: {
      type: Array,
      value: null,
      notify: true,
    },
  },

  /**
   * @return {boolean}
   * @private
   */
  isGooglePhotos_() {
    return this.topicSource === AmbientModeTopicSource.GOOGLE_PHOTOS;
  }
});
