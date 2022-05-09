// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying options to select a
 * AmbientModeTopicSource in a list.
 */

import '//resources/cr_elements/cr_radio_button/cr_radio_button_style_css.m.js';
import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import '//resources/cr_elements/shared_style_css.m.js';
import '//resources/cr_elements/shared_vars_css.m.js';
import '../../settings_shared_css.js';

import {I18nBehavior, I18nBehaviorInterface} from '//resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AmbientModeTopicSource} from './constants.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const TopicSourceItemElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
class TopicSourceItemElement extends TopicSourceItemElementBase {
  static get is() {
    return 'topic-source-item';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Whether this item is selected. This property is related to
       * cr_radio_button_style and used to style the disc appearance.
       */
      checked: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      hasGooglePhotosAlbums: {
        type: Boolean,
        value: false,
      },

      /** @type {!AmbientModeTopicSource} */
      item: Object,

      buttonLabel: {
        type: String,
        computed: 'getButtonLabel_(item)',
      },

      /** Aria label for the row. */
      ariaLabel: {
        type: String,
        computed: 'computeAriaLabel_(item, checked)',
        reflectToAttribute: true,
      },
    };
  }

  ready() {
    super.ready();

    this.addEventListener('keydown', this.onKeydown_);
  }

  /**
   * @return {string}
   * @private
   */
  getItemName_() {
    if (this.item === AmbientModeTopicSource.GOOGLE_PHOTOS) {
      return this.i18n('ambientModeTopicSourceGooglePhotos');
    } else if (this.item === AmbientModeTopicSource.ART_GALLERY) {
      return this.i18n('ambientModeTopicSourceArtGallery');
    } else {
      return '';
    }
  }

  /**
   * @return {string}
   * @private
   */
  getItemDescription_() {
    if (this.item === AmbientModeTopicSource.GOOGLE_PHOTOS) {
      if (this.hasGooglePhotosAlbums) {
        return this.i18n('ambientModeTopicSourceGooglePhotosDescription');
      } else {
        return this.i18n(
            'ambientModeTopicSourceGooglePhotosDescriptionNoAlbum');
      }
    } else if (this.item === AmbientModeTopicSource.ART_GALLERY) {
      return this.i18n('ambientModeTopicSourceArtGalleryDescription');
    } else {
      return '';
    }
  }

  /**
   * The aria label for the subpage button.
   * @return {string}
   * @private
   */
  getButtonLabel_() {
    return this.i18n('ambientModeTopicSourceSubpage', this.getItemName_());
  }

  /**
   * @return {string} Aria label string for ChromeVox to verbalize.
   * @private
   */
  computeAriaLabel_() {
    if (this.checked) {
      return this.i18n(
          'ambientModeTopicSourceSelectedRow', this.getItemName_(),
          this.getItemDescription_());
    }
    return this.i18n(
        'ambientModeTopicSourceUnselectedRow', this.getItemName_(),
        this.getItemDescription_());
  }

  /**
   * @param {!Event} event
   * @private
   */
  onKeydown_(/** @type {!KeyboardEvent}*/ event) {
    // The only key event handled by this element is pressing Enter.
    // Pressing anywhere leads to the subpage.
    if (event.key !== 'Enter') {
      return;
    }

    this.fireShowAlbums_();
    event.preventDefault();
    event.stopPropagation();
  }

  /**
   * @param {!MouseEvent} event
   * @private
   */
  onItemClick_(event) {
    // Clicking anywhere leads to the subpage.
    this.fireShowAlbums_();
    event.stopPropagation();
  }

  /**
   * @param {!MouseEvent} event
   * @private
   */
  onSubpageArrowClick_(event) {
    this.fireShowAlbums_();
    event.stopPropagation();
  }

  /**
   * Fires a 'show-albums' event with |this.item| as the details.
   * @private
   */
  fireShowAlbums_() {
    const event = new CustomEvent(
        'show-albums', {bubbles: true, composed: true, detail: this.item});
    this.dispatchEvent(event);
  }
}

customElements.define(TopicSourceItemElement.is, TopicSourceItemElement);
