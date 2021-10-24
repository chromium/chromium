// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * The breadcrumb that displays the current view stack and allows users to
 * navigate.
 */

import '/strings.m.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../common/styles.js';
import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {isNonEmptyArray} from '../common/utils.js';
import {Paths} from './personalization_router_element.js';
import {WithPersonalizationStore} from './personalization_store.js';

/** @polymer */
export class WallpaperBreadcrumb extends WithPersonalizationStore {
  static get is() {
    return 'wallpaper-breadcrumb';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * The current collection id to display.
       */
      collectionId: {
        type: String,
      },

      /**
       * The current path of the page.
       */
      path: {
        type: String,
      },

      /**
       * @private
       * @type {?Array<!ash.personalizationApp.mojom.WallpaperCollection>}
       */
      collections_: {
        type: Array,
      },

      /** @private */
      pageLabel_: {
        type: String,
        computed: 'computePageLabel_(path, collections_, collectionId)',
      },

      /** @private */
      showBackButton_: {
        type: Boolean,
        computed: 'computeShowBackButton_(path)',
      },
    };
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    this.watch('collections_', state => state.backdrop.collections);
    this.updateFromStore();
  }

  /**
   * @private
   * @param {string} path
   * @param {?Array<!ash.personalizationApp.mojom.WallpaperCollection>}
   *     collections
   * @param {string} collectionId
   * @return {string}
   */
  computePageLabel_(path, collections, collectionId) {
    switch (path) {
      case Paths.CollectionImages:
        if (!isNonEmptyArray(collections)) {
          return '';
        }
        const collection =
            collections.find(collection => collection.id === collectionId);
        return collection ? collection.name : '';
      case Paths.GooglePhotosCollection:
        return this.i18n('googlePhotosLabel');
      case Paths.LocalCollection:
        return this.i18n('myImagesLabel');
      default:
        return '';
    }
  }

  /**
   * @private
   * @param {string} path
   * @returns {boolean}
   */
  computeShowBackButton_(path) {
    return path !== Paths.Collections;
  }

  /**
   * @private
   * @return {string}
   */
  getBackButtonAriaLabel_() {
    return this.i18n('back', this.i18n('title'));
  }

  /** @private */
  onBackClick_() {
    window.history.back();
  }
}

customElements.define(WallpaperBreadcrumb.is, WallpaperBreadcrumb);
