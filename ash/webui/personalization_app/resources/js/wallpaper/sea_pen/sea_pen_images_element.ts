// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A polymer component that displays the result set of SeaPen
 * wallpapers.
 */

import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/icons.html.js';
import '../../../css/common.css.js';
import '../../../css/sea_pen.css.js';
import 'chrome://resources/ash/common/sea_pen/surface_effects/sparkle_placeholder.js';

import {MantaStatusCode, SeaPenThumbnail} from 'chrome://resources/ash/common/sea_pen/sea_pen.mojom-webui.js';
import {isNonEmptyArray} from 'chrome://resources/ash/common/sea_pen/sea_pen_utils.js';

import {selectSeaPenWallpaper} from './sea_pen_controller.js';
import {getTemplate} from './sea_pen_images_element.html.js';
import {getSeaPenProvider} from './sea_pen_interface_provider.js';
import {WithSeaPenStore} from './sea_pen_store.js';

export class SeaPenImagesElement extends WithSeaPenStore {
  static get is() {
    return 'sea-pen-images';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      templateId: String,

      thumbnails_: Object,

      thumbnailsLoading_: Boolean,

      // The pending selected image. Not persisted in store as it is only
      // temporarily available in this element.
      pendingSelected_: Object,

      thumbnailResponseStatusCode_: {
        type: Object,
        value: null,
      },

      showError_: {
        type: Boolean,
        computed:
            'computeShowError_(thumbnailResponseStatusCode_, thumbnailsLoading_)',
      },
    };
  }

  private templateId: string;
  private thumbnails_: SeaPenThumbnail[]|null;
  private thumbnailsLoading_: boolean;
  private pendingSelected_: SeaPenThumbnail|null;
  private thumbnailResponseStatusCode_: MantaStatusCode|null;
  private showError_: boolean;


  override connectedCallback() {
    super.connectedCallback();
    this.watch<SeaPenImagesElement['thumbnails_']>(
        'thumbnails_', state => state.thumbnails);
    this.watch<SeaPenImagesElement['thumbnailsLoading_']>(
        'thumbnailsLoading_', state => state.loading.thumbnails);
    this.watch<SeaPenImagesElement['thumbnailResponseStatusCode_']>(
        'thumbnailResponseStatusCode_',
        state => state.thumbnailResponseStatusCode);
    this.updateFromStore();
  }

  private getThumbnailPlaceholderClass_(thumbnailsLoading: boolean): string {
    // TODO(b/299108994): change placeholder to other loading class and add
    // loading effect style.
    return thumbnailsLoading ? 'thumbnail-placeholder placeholder' :
                               'thumbnail-placeholder';
  }
  private computeShowError_(
      statusCode: MantaStatusCode|null, thumbnailsLoading: boolean): boolean {
    return !!statusCode && !thumbnailsLoading;
  }

  private shouldShowThumbnailPlaceholders_(
      thumbnailsLoading: boolean, thumbnails: SeaPenThumbnail[]|null): boolean {
    // Use placeholders before and during loading thumbnails.
    return !thumbnails && !thumbnailsLoading;
  }

  private shouldShowImageThumbnails_(
      thumbnailsLoading: boolean, thumbnails: SeaPenThumbnail[]|null): boolean {
    return !thumbnailsLoading && isNonEmptyArray(thumbnails);
  }

  private getPlaceholders_(x: number) {
    return new Array(x).fill(0);
  }

  private onThumbnailSelected_(event: Event&{model: {item: SeaPenThumbnail}}) {
    this.pendingSelected_ = event.model.item;
    selectSeaPenWallpaper(
        event.model.item, getSeaPenProvider(), this.getStore());
  }

  private getAriaIndex_(i: number): number {
    return i + 1;
  }

  private isThumbnailSelected_(
      thumbnail: SeaPenThumbnail, pendingSelected: SeaPenThumbnail|null) {
    return thumbnail === pendingSelected;
  }

  private onClickThumbsUp_() {
    // TODO(b/313667113): Implement thumbs up.
  }

  private onClickThumbsDown_() {
    // TODO(b/313667113): Implement thumbs down.
  }
}

customElements.define(SeaPenImagesElement.is, SeaPenImagesElement);
