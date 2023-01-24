// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A polymer element that previews ambient settings in a large
 * card. Shows an album cover image, some additional preview images in a
 * collage, and informational text about the selected albums. Currently used on
 * the personalization app main page.
 */

import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import './ambient_zero_state_svg_element.js';
import '../../css/common.css.js';
import '../../css/cros_button_style.css.js';

import {assert} from 'chrome://resources/js/assert_ts.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import {TopicSource} from '../personalization_app.mojom-webui.js';
import {logAmbientModeOptInUMA} from '../personalization_metrics_logger.js';
import {Paths, PersonalizationRouter} from '../personalization_router_element.js';
import {isNonEmptyArray} from '../utils.js';

import {setAmbientModeEnabled} from './ambient_controller.js';
import {getAmbientProvider} from './ambient_interface_provider.js';
import {AmbientPreviewBase} from './ambient_preview_base.js';
import {getTemplate} from './ambient_preview_large_element.html.js';

export class AmbientPreviewLarge extends AmbientPreviewBase {
  static get is() {
    return 'ambient-preview-large';
  }

  static get template() {
    return getTemplate();
  }

  static override get properties() {
    return {
      googlePhotosAlbumsPreviews_: {
        type: Array,
        value: null,
      },
      collageImages_: {
        type: Array,
        computed:
            'computeCollageImages_(topicSource_, previewAlbums_, googlePhotosAlbumsPreviews_)',
      },
    };
  }

  private collageImages_: Url[];

  /**
   * Return the array of images that form the collage.
   * When topic source is Google Photos:
   *   - if `googlePhotosAlbumsPreviews_` is non-empty but contains fewer than 4
   *     images, only return one of them; otherwise return the first 4.
   *   - if `googlePhotosAlbumsPreviews_` is empty:
   *        - e.g. user selected art gallery albums
   *        - if `previewAlbums_` contains fewer than 4 albums, return one of
   *        their previews; otherwise return the first 4.
   *
   * If isAmbientSubpageUiChangeEnabled flag is on, max number of collage image
   * will be 3 instead of 4.
   */
  private computeCollageImages_(): Url[] {
    const maxLength = this.isAmbientSubpageUiChangeEnabled_ ? 3 : 4;
    switch (this.topicSource_) {
      case TopicSource.kArtGallery:
        return (this.previewAlbums_ || [])
            .map(album => album.url)
            .slice(0, maxLength);
      case TopicSource.kGooglePhotos:
        if (isNonEmptyArray(this.googlePhotosAlbumsPreviews_)) {
          return this.googlePhotosAlbumsPreviews_.length < maxLength ?
              [this.googlePhotosAlbumsPreviews_[0]] :
              this.googlePhotosAlbumsPreviews_.slice(0, maxLength);
        }
        if (isNonEmptyArray(this.previewAlbums_)) {
          return this.previewAlbums_.length < maxLength ?
              [this.previewAlbums_[0].url] :
              this.previewAlbums_.map(album => album.url).slice(0, maxLength);
        }
    }
    return [];
  }

  private onClickAmbientSubpageLink_() {
    PersonalizationRouter.instance().goToRoute(Paths.AMBIENT);
  }

  /** Enable ambient mode and navigates to the ambient subpage. */
  private async onClickAmbientModeButton_(event: Event) {
    assert(this.ambientModeEnabled_ === false);
    event.stopPropagation();
    logAmbientModeOptInUMA();
    await setAmbientModeEnabled(
        /*ambientModeEnabled=*/ true, getAmbientProvider(), this.getStore());
    PersonalizationRouter.instance().goToRoute(Paths.AMBIENT);
  }

  /** Navigates to the ambient subpage. */
  private onClickPreviewImage_(event: Event) {
    event.stopPropagation();
    PersonalizationRouter.instance().goToRoute(Paths.AMBIENT);
  }

  /**
   * Navigate directly to photo selection subpage. Should only be possible to
   * call this function if |topic_source| is set and photo collage is visible.
   */
  private onClickPhotoCollage_(event: Event) {
    assert(typeof this.topicSource_ === 'number', 'topic source required');
    event.stopPropagation();
    PersonalizationRouter.instance().selectAmbientAlbums(this.topicSource_);
  }

  private getThumbnailContainerClass_(): string {
    return `thumbnail-${this.collageImages_.length} clickable`;
  }

  private getCollageContainerClass_(): string {
    return `collage-${this.collageImages_.length} clickable`;
  }
}

customElements.define(AmbientPreviewLarge.is, AmbientPreviewLarge);
