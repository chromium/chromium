// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-picture-list' is a Polymer element used to show a selectable list of
 * profile pictures plus a camera selector, file picker, and the current
 * profile image.
 */

import '//resources/cr_elements/icons.html.js';
import '//resources/cr_elements/cr_shared_style.css.js';
import '//resources/polymer/v3_0/iron-a11y-keys/iron-a11y-keys.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/polymer/v3_0/iron-selector/iron-selector.js';

import {html, Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './cr_picture_list.html.js';
import {CrPicture} from './cr_picture_types.js';
import {convertImageSequenceToPng, isEncodedPngDataUrlAnimated} from './png.js';

Polymer({
  is: 'cr-picture-list',

  _template: getTemplate(),

  properties: {
    cameraPresent: Boolean,

    /**
     * The default user images.
     * @type {!Array<!{index: number, title: string, url: string}>}
     */
    defaultImages: {
      type: Array,
      observer: 'onDefaultImagesChanged_',
    },

    /** Strings provided by host */
    chooseFileLabel: String,
    oldImageLabel: String,
    profileImageLabel: String,
    takePhotoLabel: String,

    /**
     * The currently selected item. This property is bound to the iron-selector
     * and never directly assigned. This may be undefined momentarily as
     * the selection changes due to iron-selector implementation details.
     * @private {?CrPicture.ImageElement}
     */
    selectedItem: {
      type: Object,
      notify: true,
      observer: 'onImageSelected_',
    },

    /**
     * The url of the currently set profile picture image.
     * @private
     */
    profileImageUrl_: {
      type: String,
      value: CrPicture.kDefaultImageUrl,
    },

    /**
     * The url of the old image, which is either the existing image sourced from
     * the camera or a file.
     * @private
     */
    oldImageUrl_: {
      type: String,
      value: '',
    },

    /** @private */
    selectionTypesEnum_: {
      type: Object,
      value: CrPicture.SelectionTypes,
      readOnly: true,
    },
  },

  /** @private {boolean} */
  cameraSelected_: false,

  /** @private {string} */
  selectedImageUrl_: '',

  /**
   * The fallback image to be selected when the user discards the 'old' image.
   * This may be null if the user started with the old image.
   * @private {?CrPicture.ImageElement}
   */
  fallbackImage_: null,

  setFocus() {
    if (this.selectedItem) {
      this.selectedItem.focus();
    }
  },

  onImageSelected_(newImg, oldImg) {
    if (newImg) {
      newImg.setAttribute('tabindex', '0');
      newImg.setAttribute('aria-checked', 'true');
      newImg.focus();
    }
    if (oldImg) {
      oldImg.removeAttribute('tabindex');
      oldImg.removeAttribute('aria-checked');
    }
  },

  /**
   * @param {string} imageUrl
   * @param {boolean} selected
   */
  setProfileImageUrl(imageUrl, selected) {
    this.profileImageUrl_ = imageUrl;
    this.$.profileImage.title = this.profileImageLabel;
    if (!selected) {
      return;
    }
    this.setSelectedImage_(
        /**
         * @type {!CrPicture.ImageElement}
         */
        (this.$.profileImage));
  },

  /**
   * @param {string} imageUrl
   */
  setSelectedImageUrl(imageUrl) {
    const image = this.$.selector.items.find(function(image) {
      return image.dataset.url === imageUrl;
    });
    if (image) {
      this.setSelectedImage_(
          /**
           * @type {!CrPicture.ImageElement}
           */
          (image));
      this.selectedImageUrl_ = '';
    } else {
      this.selectedImageUrl_ = imageUrl;
    }
  },

  /**
   * @param {string} imageUrl
   */
  setOldImageUrl(imageUrl) {
    if (imageUrl === CrPicture.kDefaultImageUrl) {
      // Treat the default image as empty so it does not show in the list.
      this.oldImageUrl_ = '';
      this.setSelectedImageUrl(CrPicture.kDefaultImageUrl);
      return;
    }
    this.oldImageUrl_ = imageUrl;
    if (imageUrl) {
      this.$.selector.select(this.$.selector.indexOf(this.$.oldImage));
      this.selectedImageUrl_ = imageUrl;
    } else if (this.cameraSelected_) {
      this.$.selector.select(this.$.selector.indexOf(this.$.cameraImage));
    } else if (
        this.fallbackImage_ &&
        this.fallbackImage_.dataset.type !== CrPicture.SelectionTypes.OLD) {
      this.selectImage_(
          /**
           * @type {!CrPicture.ImageElement}
           */
          (this.fallbackImage_), true /* activate */);
    } else {
      this.selectImage_(
          /**
           * @type {!CrPicture.ImageElement}
           */
          (this.$.profileImage), true /* activate */);
    }
  },

  /**
   * Handler for when accessibility-specific keys are pressed.
   * @param {!CustomEvent<!{key: string, keyboardEvent: Object}>} e
   */
  onKeysPressed(e) {
    if (!this.selectedItem) {
      return;
    }

    const selector = /** @type {IronSelectorElement} */ (this.$.selector);
    const prevSelected = this.selectedItem;
    let activate = false;
    switch (e.detail.key) {
      case 'enter':
      case 'space':
        activate = true;
        break;
      case 'up':
      case 'left':
        do {
          selector.selectPrevious();
        } while (this.selectedItem.hidden &&
                 this.selectedItem !== prevSelected);
        break;
      case 'down':
      case 'right':
        do {
          selector.selectNext();
        } while (this.selectedItem.hidden &&
                 this.selectedItem !== prevSelected);
        break;
      default:
        return;
    }
    this.selectImage_(this.selectedItem, activate);
    e.detail.keyboardEvent.preventDefault();
  },

  /**
   * @param {!CrPicture.ImageElement} image
   */
  setSelectedImage_(image) {
    this.fallbackImage_ = image;
    // If the user is currently taking a photo, do not change the focus.
    if (!this.selectedItem ||
        this.selectedItem.dataset.type !== CrPicture.SelectionTypes.CAMERA) {
      this.$.selector.select(this.$.selector.indexOf(image));
      this.selectedItem = image;
    }
  },

  /** @private */
  onDefaultImagesChanged_() {
    if (this.selectedImageUrl_) {
      this.setSelectedImageUrl(this.selectedImageUrl_);
    }
  },

  /**
   * @param {!CrPicture.ImageElement} selected
   * @param {boolean} activate
   * @private
   */
  selectImage_(selected, activate) {
    this.cameraSelected_ =
        selected.dataset.type === CrPicture.SelectionTypes.CAMERA;
    this.selectedItem = selected;

    if (selected.dataset.type === CrPicture.SelectionTypes.CAMERA) {
      if (activate) {
        this.fire('focus-action', selected);
      }
    } else if (
        activate || selected.dataset.type !== CrPicture.SelectionTypes.FILE) {
      this.fire('image-activate', selected);
    }
  },

  /**
   * @param {!Event} event
   * @private
   */
  onIronActivate_(event) {
    event.stopPropagation();
    const type = event.detail.item.dataset.type;
    // Don't change focus when activating the camera via mouse.
    const activate = type !== CrPicture.SelectionTypes.CAMERA;
    this.selectImage_(event.detail.item, activate);
  },

  /**
   * @param {!Event} event
   * @private
   */
  onIronSelect_(event) {
    event.stopPropagation();
  },

  /**
   * @param {!Event} event
   * @private
   */
  onSelectedItemChanged_(event) {
    if (event.target.selectedItem) {
      event.target.selectedItem.scrollIntoViewIfNeeded(false);
    }
  },

  /**
   * Returns the image to use for 'src'.
   * @param {string} url
   * @return {string}
   * @private
   */
  getImgSrc_(url) {
    // Use first frame of animated user images.
    if (url.startsWith('chrome://theme')) {
      return url + '[0]';
    }

    /**
     * Extract first frame from image by creating a single frame PNG using
     * url as input if base64 encoded and potentially animated.
     */
    if (url.split(',')[0] === 'data:image/png;base64') {
      return convertImageSequenceToPng([url]);
    }

    return url;
  },

  /**
   * Returns the 2x (high dpi) image to use for 'srcset' for chrome://theme
   * images. Note: 'src' will still be used as the 1x candidate as per the HTML
   * spec.
   * @param {string} url
   * @return {string}
   * @private
   */
  getImgSrc2x_(url) {
    if (!url.startsWith('chrome://theme')) {
      return '';
    }
    return url + '[0]@2x 2x';
  },
});
