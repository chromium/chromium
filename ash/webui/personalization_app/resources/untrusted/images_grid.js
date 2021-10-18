// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://personalization/polymer/v3_0/iron-list/iron-list.js';
import './setup.js';
import './styles.js';
import {html, PolymerElement} from 'chrome-untrusted://personalization/polymer/v3_0/polymer/polymer_bundled.min.js';
import {EventType} from '../common/constants.js';
import {selectImage, validateReceivedData} from '../common/iframe_api.js';
import {isSelectionEvent} from '../common/utils.js';

/**
 * @fileoverview Responds to |SendImagesEvent| from trusted. Handles user input
 * and responds with |SelectImageEvent| when an image is selected.
 */

class ImagesGrid extends PolymerElement {
  static get is() {
    return 'images-grid';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * @type {!Array<!chromeos.personalizationApp.mojom.WallpaperImage>}
       * @private
       */
      images_: {
        type: Array,
        value: [],
      },

      /**
       * @type {?bigint}
       * @private
       */
      selectedAssetId_: {
        type: Object,
        value: null,
      },

      /**
       * @type {?bigint}
       * @private
       */
      pendingSelectedAssetId_: {
        type: Object,
        value: null,
      }
    };
  }

  constructor() {
    super();
    this.onMessageReceived_ = this.onMessageReceived_.bind(this);
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    window.addEventListener('message', this.onMessageReceived_);
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();
    window.removeEventListener('message', this.onMessageReceived_);
  }

  /**
   * Handler for messages from trusted code. Expects only SendImagesEvent and
   * will error on any other event.
   * @param {!Event} message
   * @private
   */
  onMessageReceived_(message) {
    switch (message.data.type) {
      case EventType.SEND_IMAGES:
        try {
          this.images_ = validateReceivedData(message, EventType.SEND_IMAGES);
        } catch (e) {
          console.warn('Invalid images received', e);
          this.images_ = [];
        }
        return;
      case EventType.SEND_CURRENT_WALLPAPER_ASSET_ID:
        this.selectedAssetId_ = validateReceivedData(
            message, EventType.SEND_CURRENT_WALLPAPER_ASSET_ID);
        return;
      case EventType.SEND_PENDING_WALLPAPER_ASSET_ID:
        this.pendingSelectedAssetId_ = validateReceivedData(
          message, EventType.SEND_PENDING_WALLPAPER_ASSET_ID);
        return;
      case EventType.SEND_VISIBLE:
        const visible = validateReceivedData(message, EventType.SEND_VISIBLE);
        if (!visible) {
          // When the iframe is hidden, do some dom magic to hide old image
          // content. This is in preparation for a user switching to a new
          // wallpaper collection and loading a new set of images.
          const ironList = this.shadowRoot.querySelector('iron-list');
          const images = ironList.querySelectorAll('.photo-container img');
          for (const image of images) {
            image.src = '';
          }
          this.images_ = [];
        }
        return;
      default:
        throw new Error('unexpected event type');
    }
  }

  /**
   * @param {!chromeos.personalizationApp.mojom.WallpaperImage} image
   * @param {?bigint} selectedAssetId
   * @param {?bigint} pendingSelectedAssetId
   * @return {string}
   */
  getAriaSelected_(image, selectedAssetId, pendingSelectedAssetId) {
    // Make sure that both are bigint (not undefined) and equal.
    return (typeof selectedAssetId === 'bigint' &&
            image.assetId === selectedAssetId && !pendingSelectedAssetId ||
            typeof pendingSelectedAssetId === 'bigint' &&
            image.assetId === pendingSelectedAssetId)
        .toString();
  }

  /**
   * Notify trusted code that a user selected an image.
   * @private
   * @param {!Event} e
   */
  onImageSelected_(e) {
    if (!isSelectionEvent(e)) {
      return;
    }
    const assetId = BigInt(e.currentTarget.dataset.id);
    selectImage(window.parent, assetId);
  }

  /**
   * @private
   * @param {!chromeos.personalizationApp.mojom.WallpaperImage} image
   * @return {string}
   */
  getAriaLabel_(image) {
    return image.attribution.join(' ');
  }

  /**
   * @private
   * @param {number} i
   * @return {number}
   */
  getAriaIndex_(i) {
    return i + 1;
  }
}

customElements.define(ImagesGrid.is, ImagesGrid);
