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

import {TopicSource} from '../../personalization_app.mojom-webui.js';
import {logAmbientModeOptInUMA} from '../personalization_metrics_logger.js';
import {Paths, PersonalizationRouter, ScrollableTarget} from '../personalization_router_element.js';
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
      previewImages_: {
        type: Array,
        value: null,
      },
      collageImages_: {
        type: Array,
        computed:
            'computeCollageImages_(topicSource_, previewAlbums_, previewImages_)',
      },
      thumbnailImages_: {
        type: Array,
        computed: 'computeThumbnailImages_(topicSource_, previewImages_)',
      },
    };
  }

  private collageImages_: Url[];
  private thumbnailImages_: Url[];

  /** Returns the array of images that form the collage when Jelly is off. */
  private computeCollageImages_(): Url[] {
    switch (this.topicSource_) {
      case TopicSource.kVideo:
        return this.previewImages_ || [];
      case TopicSource.kArtGallery:
        return (this.previewAlbums_ || []).map(album => album.url).slice(0, 2);
      case TopicSource.kGooglePhotos:
        const maxLength = 4;
        if (isNonEmptyArray(this.previewImages_)) {
          return this.previewImages_.length < maxLength ?
              [this.previewImages_[0]] :
              this.previewImages_.slice(0, maxLength);
        }
        if (isNonEmptyArray(this.previewAlbums_)) {
          return this.previewAlbums_.length < maxLength ?
              [this.previewAlbums_[0].url] :
              this.previewAlbums_.map(album => album.url).slice(0, maxLength);
        }
    }
    return [];
  }

  /** Returns the array of thumbnail images. */
  private computeThumbnailImages_(): Url[] {
    if (isNonEmptyArray(this.previewImages_)) {
      const maxLength = Math.min(
          this.previewImages_.length,
          this.topicSource_ === TopicSource.kArtGallery ? 2 : 3);
      return this.previewImages_.slice(0, maxLength);
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
   * Navigate to ambient subpage and scroll down to image source section.
   */
  private onClickThumbnails_(event: Event) {
    event.stopPropagation();
    PersonalizationRouter.instance().goToRoute(
        Paths.AMBIENT, {scrollTo: ScrollableTarget.TOPIC_SOURCE_LIST});
  }

  /**
   * Navigate directly to photo selection subpage. Should only be possible to
   * call this function if |topic_source| is set and thumbnail is visible.
   */
  private onClickPhotoCollage_(event: Event) {
    assert(typeof this.topicSource_ === 'number', 'topic source required');
    event.stopPropagation();
    PersonalizationRouter.instance().selectAmbientAlbums(this.topicSource_);
  }

  private getThumbnailContainerClass_(): string {
    return `thumbnail-${this.thumbnailImages_.length} clickable`;
  }

  private getCollageContainerClass_(): string {
    return `collage-${this.collageImages_.length} clickable`;
  }
}

customElements.define(AmbientPreviewLarge.is, AmbientPreviewLarge);
