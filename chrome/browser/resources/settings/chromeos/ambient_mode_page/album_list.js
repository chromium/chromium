// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying a list of photo previews.
 */

/**
 * Polymer class definition for 'album-list'.
 */
import './album_item.js';
import '//resources/cr_elements/shared_style_css.m.js';
import '//resources/cr_elements/shared_vars_css.m.js';
import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '//resources/polymer/v3_0/iron-list/iron-list.js';
import '//resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import '../../settings_shared_css.js';

import {html, Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {GlobalScrollTargetBehavior} from '../global_scroll_target_behavior.js';
import {routes} from '../os_route.js';

import {AmbientModeAlbum, AmbientModeTopicSource} from './constants.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'album-list',

  behaviors: [
    GlobalScrollTargetBehavior,
  ],

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

    /**
     * Needed by GlobalScrollTargetBehavior.
     * @override
     */
    subpageRoute: {
      type: Object,
      value: routes.AMBIENT_MODE_PHOTOS,
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
