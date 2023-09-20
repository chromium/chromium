// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A polymer component that displays the result set of SeaPen
 * wallpapers.
 */

import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import '../../../css/common.css.js';

import {WithPersonalizationStore} from '../../personalization_store.js';
import {getZerosArray, isNonEmptyArray} from '../../utils.js';
import {WallpaperSearchThumbnail} from '../constants.js';

import {getTemplate} from './sea_pen_images_element.html.js';

export class SeaPenImagesElement extends WithPersonalizationStore {
  static get is() {
    return 'sea-pen-images';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      templateId: String,

      query_: String,

      thumbnails_: Object,

      thumbnailsLoading_: Boolean,
    };
  }

  private templateId: string;
  private query_: string|null;
  private thumbnails_: WallpaperSearchThumbnail[]|null;
  private thumbnailsLoading_: boolean;

  override connectedCallback() {
    super.connectedCallback();
    this.watch<SeaPenImagesElement['query_']>(
        'query_', state => state.wallpaper.seaPen.query);
    this.watch<SeaPenImagesElement['thumbnails_']>(
        'thumbnails_', state => state.wallpaper.seaPen.thumbnails);
    this.watch<SeaPenImagesElement['thumbnailsLoading_']>(
        'thumbnailsLoading_',
        state => state.wallpaper.seaPen.thumbnailsLoading);
    this.updateFromStore();
  }

  private getThumbnailPlaceholderClass_(thumbnailsLoading: boolean): string {
    // TODO(b/299108994): change placeholder to other loading class and add
    // loading effect style.
    return thumbnailsLoading ? 'thumbnail-placeholder placeholder' :
                               'thumbnail-placeholder';
  }

  private shouldShowThumbnailPlaceholders_(
      thumbnailsLoading: boolean,
      thumbnails: WallpaperSearchThumbnail[]|null): boolean {
    // Use placeholders before and during loading thumbnails.
    return !thumbnails || thumbnailsLoading;
  }

  private shouldShowImageThumbnails_(
      thumbnailsLoading: boolean,
      thumbnails: WallpaperSearchThumbnail[]|null): boolean {
    return !thumbnailsLoading && isNonEmptyArray(thumbnails);
  }

  private getPlaceholders_(x: number) {
    return getZerosArray(x);
  }
}

customElements.define(SeaPenImagesElement.is, SeaPenImagesElement);
