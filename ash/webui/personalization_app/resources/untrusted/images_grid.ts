// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://personalization/polymer/v3_0/iron-list/iron-list.js';
import './setup.js';
import './styles.js';

import {html, PolymerElement} from 'chrome-untrusted://personalization/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assert, assertNotReached} from '../common/assert.m.js';
import {Events, EventType, ImageTile} from '../common/constants.js';
import {isSelectionEvent} from '../common/utils.js';
import {selectImage, validateReceivedData} from '../untrusted/iframe_api.js';

/**
 * @fileoverview Responds to |SendImageTilesEvent| from trusted. Handles user
 * input and responds with |SelectImageEvent| when an image is selected.
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
      tiles_: {
        type: Array,
        value: [],
      },

      selectedAssetId_: {
        type: Object,
        value: null,
      },

      pendingSelectedAssetId_: {
        type: Object,
        value: null,
      }
    };
  }

  private tiles_: ImageTile[];
  private selectedAssetId_: bigint|undefined;
  private pendingSelectedAssetId_: bigint|undefined;

  constructor() {
    super();
    this.onMessageReceived_ = this.onMessageReceived_.bind(this);
  }

  connectedCallback() {
    super.connectedCallback();
    window.addEventListener('message', this.onMessageReceived_);
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    window.removeEventListener('message', this.onMessageReceived_);
  }

  /**
   * Handler for messages from trusted code. Expects only SendImagesEvent and
   * will error on any other event.
   */
  private onMessageReceived_(message: MessageEvent) {
    const event: Events = message.data;
    switch (event.type) {
      case EventType.SEND_IMAGE_TILES:
        this.tiles_ =
            validateReceivedData(event, message.origin) ? event.tiles : [];
        return;
      case EventType.SEND_CURRENT_WALLPAPER_ASSET_ID:
        this.selectedAssetId_ = validateReceivedData(event, message.origin) ?
            event.assetId :
            undefined;
        return;
      case EventType.SEND_PENDING_WALLPAPER_ASSET_ID:
        this.pendingSelectedAssetId_ =
            validateReceivedData(event, message.origin) ? event.assetId :
                                                          undefined;
        return;
      case EventType.SEND_VISIBLE:
        let visible = false;
        if (validateReceivedData(event, message.origin)) {
          visible = event.visible;
        }
        if (!visible) {
          // When the iframe is hidden, do some dom magic to hide old image
          // content. This is in preparation for a user switching to a new
          // wallpaper collection and loading a new set of images.
          const ironList = this.shadowRoot!.querySelector('iron-list');
          const images: NodeListOf<HTMLImageElement> =
              ironList!.querySelectorAll('.photo-container img');
          for (const image of images) {
            image.src = '';
          }
          this.tiles_ = [];
        }
        return;
      default:
        throw new Error('unexpected event type');
    }
  }

  private getAriaSelected_(
      tile: ImageTile, selectedAssetId: bigint|undefined,
      pendingSelectedAssetId: bigint|undefined): string {
    // Make sure that both are bigint (not undefined) and equal.
    return (typeof selectedAssetId === 'bigint' &&
                tile?.assetId === selectedAssetId && !pendingSelectedAssetId ||
            typeof pendingSelectedAssetId === 'bigint' &&
                tile?.assetId === pendingSelectedAssetId)
        .toString();
  }

  private getClassForImg_(index: number, tile: ImageTile): string {
    if (tile.preview.length < 2) {
      return '';
    }
    switch (index) {
      case 0:
        return 'left';
      case 1:
        return 'right';
      default:
        return '';
    }
  }

  /**
   * Notify trusted code that a user selected an image.
   */
  private onImageSelected_(e: Event) {
    if (!isSelectionEvent(e)) {
      return;
    }
    const imgElement = e.currentTarget as HTMLImageElement;
    const assetId = imgElement.dataset['assetId'];
    if (assetId === undefined) {
      assertNotReached('assetId not found');
      return;
    }
    selectImage(window.parent, BigInt(assetId));
  }

  private getAriaLabel_(tile: ImageTile): string {
    return tile.attribution.join(' ');
  }

  private getAriaIndex_(i: number): number {
    return i + 1;
  }
}

customElements.define(ImagesGrid.is, ImagesGrid);
