// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A polymer element that previews ambient settings in a large
 * card. Shows an album cover image, some additional preview images in a
 * collage, and informational text about the selected albums. Currently used on
 * the personalization app main page.
 */

import 'chrome://resources/ash/common/personalization/common.css.js';
import 'chrome://resources/ash/common/personalization/cros_button_style.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_auto_img/cr_auto_img.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import './ambient_zero_state_svg_element.js';

import {isNonEmptyArray} from 'chrome://resources/ash/common/sea_pen/sea_pen_utils.js';
import {assert} from 'chrome://resources/js/assert.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import {TopicSource} from '../../personalization_app.mojom-webui.js';
import {logAmbientModeOptInUMA} from '../personalization_metrics_logger.js';
import {Paths, PersonalizationRouterElement, ScrollableTarget} from '../personalization_router_element.js';

import {setAmbientModeEnabled} from './ambient_controller.js';
import {getAmbientProvider} from './ambient_interface_provider.js';
import {AmbientPreviewBase} from './ambient_preview_base.js';
import {getTemplate} from './ambient_preview_large_element.html.js';

export class AmbientPreviewLargeElement extends AmbientPreviewBase {
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
      thumbnailImages_: {
        type: Array,
        computed: 'computeThumbnailImages_(topicSource_, previewImages_)',
      },
    };
  }

  private thumbnailImages_: Url[];

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
    PersonalizationRouterElement.instance().goToRoute(Paths.AMBIENT);
  }

  /** Enable ambient mode and navigates to the ambient subpage. */
  private async onClickAmbientModeButton_(event: Event) {
    assert(this.ambientModeEnabled_ === false);
    event.stopPropagation();
    logAmbientModeOptInUMA();
    await setAmbientModeEnabled(
        /*ambientModeEnabled=*/ true, getAmbientProvider(), this.getStore());
    PersonalizationRouterElement.instance().goToRoute(Paths.AMBIENT);
  }

  /** Navigates to the ambient subpage. */
  private onClickPreviewImage_(event: Event) {
    event.stopPropagation();
    PersonalizationRouterElement.instance().goToRoute(Paths.AMBIENT);
  }

  /**
   * Navigate to ambient subpage and scroll down to image source section.
   */
  private onClickThumbnails_(event: Event) {
    event.stopPropagation();
    PersonalizationRouterElement.instance().goToRoute(
        Paths.AMBIENT, {scrollTo: ScrollableTarget.TOPIC_SOURCE_LIST});
  }

  private getThumbnailContainerClass_(): string {
    return `thumbnail-${this.thumbnailImages_.length} clickable`;
  }
}

customElements.define(
    AmbientPreviewLargeElement.is, AmbientPreviewLargeElement);
