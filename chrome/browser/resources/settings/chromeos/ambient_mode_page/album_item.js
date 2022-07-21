// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying photos preview in a list.
 */

import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/iron-media-query/iron-media-query.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import './text_with_tooltip.js';
import '../../settings_shared.css.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AmbientModeAlbum, AmbientModeTopicSource} from './constants.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const AlbumItemElementBase = mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
class AlbumItemElement extends AlbumItemElementBase {
  static get is() {
    return 'album-item';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {?AmbientModeAlbum} */
      album: {
        type: AmbientModeAlbum,
        value: null,
      },

      /** @private {!AmbientModeTopicSource} */
      topicSource: Number,

      /** @private */
      checked: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
        notify: true,
      },

      /** Aria label for the album. */
      ariaLabel: {
        type: String,
        computed: 'computeAriaLabel_(album.*, checked)',
        reflectToAttribute: true,
      },

      titleTooltipIsVisible: {
        type: Boolean,
        observer: 'tooltipVisibilityChanged_',
      },

      descriptionTooltipIsVisible: {
        type: Boolean,
        observer: 'tooltipVisibilityChanged_',
      },

      /**
       * Whether dark mode is the active preferred color scheme.
       * @private {boolean}
       */
      isDarkModeActive_: {
        type: Boolean,
        value: false,
      },
    };
  }

  ready() {
    super.ready();
    this.addEventListener('keydown', this.onKeydown_);
  }

  /**
   * @return {string} Class name of album type.
   * @private
   */
  computeClass_() {
    return this.topicSource === AmbientModeTopicSource.GOOGLE_PHOTOS ?
        'personal-album' :
        'art-album';
  }

  /**
   * @return {string} Checked icon for album.
   * @private
   */
  computeCheckedIcon_() {
    return this.isDarkModeActive_ ? 'os-settings:ic-checked-filled-dark' :
                                    'os-settings:ic-checked-filled';
  }

  /**
   * @return {string} Aria label string for ChromeVox to verbalize.
   * @private
   */
  computeAriaLabel_() {
    if (!this.album) {
      return '';
    }

    if (this.album.checked) {
      return this.i18n(
          'ambientModeAlbumsSubpageAlbumSelected', this.album.title,
          this.album.description);
    }

    return this.i18n(
        'ambientModeAlbumsSubpageAlbumUnselected', this.album.title,
        this.album.description);
  }

  /**
   * @param {!Event} event
   * @private
   */
  onKeydown_(/** @type {!KeyboardEvent}*/ event) {
    // The only key event handled by this element is pressing Enter.
    if (event.key !== 'Enter') {
      return;
    }

    this.fireSelectedAlbumsChanged_();
    event.preventDefault();
    event.stopPropagation();
  }

  /**
   * Because of the paper-tooltips anchored in this item exceed the bounds of
   * #albumItem and each 'grid' item in iron-list has it's own stacking
   * context, we need to adjust the z-index of the items relative to each
   * other so that the tooltip is not covered by adjacent albumItems.
   * @private
   */
  tooltipVisibilityChanged_() {
    const tooltipIsVisible =
        this.titleTooltipIsVisible || this.descriptionTooltipIsVisible;
    this.style.zIndex = tooltipIsVisible ? '1' : '0';
  }

  /**
   * @param {!MouseEvent} event
   * @private
   */
  onImageClick_(event) {
    this.fireSelectedAlbumsChanged_();
    event.preventDefault();
    event.stopPropagation();
  }

  /**
   * Fires a 'selected-albums-changed' event with |this.album| as the details.
   * @private
   */
  fireSelectedAlbumsChanged_() {
    this.checked = !this.checked;
    const event = new CustomEvent(
        'selected-albums-changed',
        {bubbles: true, composed: true, detail: this.album});
    this.dispatchEvent(event);
  }
}

customElements.define(AlbumItemElement.is, AlbumItemElement);
