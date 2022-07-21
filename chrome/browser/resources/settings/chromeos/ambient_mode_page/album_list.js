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
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import '../../settings_shared.css.js';

import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Route} from '../../router.js';
import {GlobalScrollTargetBehavior, GlobalScrollTargetBehaviorInterface} from '../global_scroll_target_behavior.js';
import {routes} from '../os_route.js';

import {AmbientModeAlbum, AmbientModeTopicSource} from './constants.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {GlobalScrollTargetBehaviorInterface}
 */
const AlbumListElementBase =
    mixinBehaviors([GlobalScrollTargetBehavior], PolymerElement);

/** @polymer */
class AlbumListElement extends AlbumListElementBase {
  static get is() {
    return 'album-list';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
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
       * @type {Route}
       * @override
       */
      subpageRoute: {
        type: Object,
        value: routes.AMBIENT_MODE_PHOTOS,
      },
    };
  }

  /**
   * @return {boolean}
   * @private
   */
  isGooglePhotos_() {
    return this.topicSource === AmbientModeTopicSource.GOOGLE_PHOTOS;
  }
}

customElements.define(AlbumListElement.is, AlbumListElement);
