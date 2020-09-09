// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying photos preview in a list.
 */

Polymer({
  is: 'album-item',

  behaviors: [I18nBehavior],

  properties: {
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
  },

  listeners: {keydown: 'onKeydown_'},

  /**
   * @return {string} Class name of album type.
   * @private
   */
  computeClass_() {
    return this.topicSource === AmbientModeTopicSource.GOOGLE_PHOTOS ?
        'personal-album' :
        'art-album';
  },

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
  },

  /**
   * @param {!KeyboardEvent} event
   * @private
   */
  onKeydown_(event) {
    // The only key event handled by this element is pressing Enter.
    if (event.key !== 'Enter') {
      return;
    }

    this.fireSelectedAlbumsChanged_();
    event.preventDefault();
    event.stopPropagation();
  },

  /**
   * @param {!MouseEvent} event
   * @private
   */
  onImageClick_(event) {
    this.fireSelectedAlbumsChanged_();
    event.preventDefault();
    event.stopPropagation();
  },

  /**
   * Fires a 'selected-albums-changed' event with |this.album| as the details.
   * @private
   */
  fireSelectedAlbumsChanged_() {
    this.checked = !this.checked;
    this.fire('selected-albums-changed', this.album);
  },

});
