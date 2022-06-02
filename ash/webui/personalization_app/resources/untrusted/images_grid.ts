// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_auto_img/cr_auto_img.js';
import '//resources/polymer/v3_0/iron-list/iron-list.js';
import './setup.js';
import '../trusted/wallpaper/trusted_style.css.js';

import {assertNotReached} from '//resources/js/assert.m.js';
import {afterNextRender, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Events, EventType, ImageTile} from '../common/constants.js';
import {getLoadingPlaceholderAnimationDelay, getLoadingPlaceholders, isSelectionEvent} from '../common/utils.js';
import {selectImage, validateReceivedData} from '../untrusted/iframe_api.js';

import {getTemplate} from './images_grid.html.js';

/**
 * @fileoverview Responds to |SendImageTilesEvent| from trusted. Handles user
 * input and responds with |SelectImageEvent| when an image is selected.
 */

export class ImagesGrid extends PolymerElement {
  static get is() {
    return 'images-grid';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      tiles_: {
        type: Array,
        value() {
          // Fill the view with loading tiles. Will be adjusted to the correct
          // number of tiles when collections are received.
          return getLoadingPlaceholders(() => 0);
        }
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

  private tiles_: ImageTile[]|number[];
  private selectedAssetId_: bigint|undefined;
  private pendingSelectedAssetId_: bigint|undefined;

  /**
   * Handler for messages from trusted code. Expects only SendImagesEvent and
   * will error on any other event.
   */
  onMessageReceived(event: Events) {
    switch (event.type) {
      case EventType.SEND_IMAGE_TILES:
        this.tiles_ = validateReceivedData(event) ? event.tiles : [];
        return;
      case EventType.SEND_CURRENT_WALLPAPER_ASSET_ID:
        this.selectedAssetId_ =
            validateReceivedData(event) ? event.assetId : undefined;
        return;
      case EventType.SEND_PENDING_WALLPAPER_ASSET_ID:
        this.pendingSelectedAssetId_ =
            validateReceivedData(event) ? event.assetId : undefined;
        return;
      case EventType.SEND_VISIBLE:
        let visible = false;
        if (validateReceivedData(event)) {
          visible = event.visible;
        }
        if (visible) {
          // If iron-list items were updated while this iron-list was hidden,
          // the layout will be incorrect. Trigger another layout when iron-list
          // becomes visible again. Wait until |afterNextRender| completes
          // otherwise iron-list width may still be 0.
          afterNextRender(this, () => {
            // Trigger a layout now that iron-list has the correct width.
            this.shadowRoot!.querySelector('iron-list')!.fire('iron-resize');
          });
        }
        return;
      default:
        throw new Error('unexpected event type');
    }
  }

  private isLoadingTile_(tile: number|ImageTile): tile is number {
    return typeof tile === 'number';
  }

  private isImageTile_(tile: number|ImageTile): tile is ImageTile {
    return tile.hasOwnProperty('preview') &&
        Array.isArray((tile as any).preview);
  }

  private getLoadingPlaceholderAnimationDelay_(index: number): string {
    return getLoadingPlaceholderAnimationDelay(index);
  }

  private getAriaSelected_(
      tile: ImageTile, selectedAssetId: bigint|undefined,
      pendingSelectedAssetId: bigint|undefined): string {
    // Make sure that both are bigint (not undefined) and equal.
    return (typeof selectedAssetId === 'bigint' && !!tile &&
                tile.assetId === selectedAssetId && !pendingSelectedAssetId ||
            typeof pendingSelectedAssetId === 'bigint' && !!tile &&
                tile.assetId === pendingSelectedAssetId)
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
    selectImage(BigInt(assetId));
  }

  private getAriaLabel_(tile: ImageTile): string {
    return tile.attribution!.join(' ');
  }

  private getAriaIndex_(i: number): number {
    return i + 1;
  }
}

customElements.define(ImagesGrid.is, ImagesGrid);
