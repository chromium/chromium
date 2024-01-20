// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A polymer component that displays the result set of SeaPen
 * wallpapers.
 */

import 'chrome://resources/ash/common/personalization/common.css.js';
import 'chrome://resources/ash/common/personalization/personalization_shared_icons.html.js';
import 'chrome://resources/ash/common/personalization/wallpaper.css.js';
import 'chrome://resources/ash/common/sea_pen/sea_pen.css.js';
import 'chrome://resources/ash/common/sea_pen/surface_effects/sparkle_placeholder.js';
import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import './sea_pen_feedback_element.js';

import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';

import {Query} from './constants.js';
import {MantaStatusCode, SeaPenTemplateId, SeaPenThumbnail} from './sea_pen.mojom-webui.js';
import {clearSeaPenThumbnails, openFeedbackDialog, selectSeaPenWallpaper} from './sea_pen_controller.js';
import {getTemplate} from './sea_pen_images_element.html.js';
import {getSeaPenProvider} from './sea_pen_interface_provider.js';
import {WithSeaPenStore} from './sea_pen_store.js';
import {isNonEmptyArray, isNonEmptyFilePath, logSeaPenTemplateFeedback} from './sea_pen_utils.js';

export class SeaPenImagesElement extends WithSeaPenStore {
  static get is() {
    return 'sea-pen-images';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      templateId: {
        type: String,
        observer: 'onTemplateIdChanged_',
      },

      thumbnails_: Object,

      thumbnailsLoading_: Boolean,

      currentSelected_: {
        type: String,
        value: null,
      },

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

  private templateId: SeaPenTemplateId|Query;
  private thumbnails_: SeaPenThumbnail[]|null;
  private thumbnailsLoading_: boolean;
  private currentSelected_: string|null;
  private pendingSelected_: SeaPenThumbnail|FilePath|null;
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
    this.watch<SeaPenImagesElement['currentSelected_']>(
        'currentSelected_', state => state.currentSelected);
    this.watch<SeaPenImagesElement['pendingSelected_']>(
        'pendingSelected_', state => state.pendingSelected);
    this.updateFromStore();
  }

  private computeShowError_(
      statusCode: MantaStatusCode|null, thumbnailsLoading: boolean): boolean {
    return !!statusCode && !thumbnailsLoading;
  }

  private getErrorMessage_(statusCode: MantaStatusCode|null): string {
    switch (statusCode) {
      case MantaStatusCode.kNoInternetConnection:
        return this.i18n('seaPenErrorNoInternet');
      case MantaStatusCode.kPerUserQuotaExceeded:
      case MantaStatusCode.kResourceExhausted:
        return this.i18n('seaPenErrorResourceExhausted');
      default:
        return this.i18n('seaPenErrorGeneric');
    }
  }

  private getErrorIllo_(statusCode: MantaStatusCode|null): string {
    switch (statusCode) {
      case MantaStatusCode.kNoInternetConnection:
        return 'personalization-shared-illo:network_error';
      default:
        return 'personalization-shared-illo:resource_error';
    }
  }

  private onTemplateIdChanged_() {
    clearSeaPenThumbnails(this.getStore());
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
    selectSeaPenWallpaper(
        event.model.item, getSeaPenProvider(), this.getStore());
  }

  private getAriaIndex_(i: number): number {
    return i + 1;
  }

  private isThumbnailSelected_(
      thumbnail: SeaPenThumbnail|undefined, currentSelected: string|null,
      pendingSelected: FilePath|SeaPenThumbnail|null): boolean {
    if (!thumbnail) {
      return false;
    }

    // Image was just clicked on and is currently being set.
    if (thumbnail === pendingSelected) {
      return true;
    }

    const fileName = `${thumbnail.id}.jpg`;

    // Image was previously selected, and was just clicked again via the "Recent
    // Images" section. This can arise if the user quickly navigates back and
    // forth from SeaPen root and results page while selecting images.
    if (isNonEmptyFilePath(pendingSelected)) {
      return pendingSelected.path.endsWith(fileName);
    }

    // No pending image in progress. Currently selected image matches the
    // thumbnail id.
    return pendingSelected === null && !!currentSelected?.endsWith(fileName);
  }

  private isThumbnailLoading_(
      thumbnail: SeaPenThumbnail|undefined,
      pendingSelected: FilePath|SeaPenThumbnail|null): boolean {
    return !!thumbnail && thumbnail === pendingSelected;
  }

  // Get the name of the template for metrics. Must match histograms.xml
  // SeaPenTemplateName.
  private getTemplateNameFromId_(templateId: SeaPenTemplateId|Query): string {
    switch (templateId) {
      case SeaPenTemplateId.kFlower:
        return 'Flower';
      case SeaPenTemplateId.kMineral:
        return 'Mineral';
      case SeaPenTemplateId.kScifi:
        return 'Scifi';
      case SeaPenTemplateId.kArt:
        return 'Art';
      case SeaPenTemplateId.kCharacters:
        return 'Characters';
      case SeaPenTemplateId.kTerrain:
        return 'Landscape';
      case SeaPenTemplateId.kCurious:
        return 'Curious';
      case SeaPenTemplateId.kDreamscapes:
        return 'Dreamscapes';
      case SeaPenTemplateId.kTranslucent:
        return 'Translucent';

      case SeaPenTemplateId.kVcBackgroundSimple:
        return 'VcBackgroundSimple';
      case SeaPenTemplateId.kVcBackgroundOffice:
        return 'VcBackgroundOffice';
      case SeaPenTemplateId.kVcBackgroundTerrainVc:
        return 'VcBackgroundTerrain';
      case SeaPenTemplateId.kVcBackgroundCafe:
        return 'VcBackgroundCafe';
      case SeaPenTemplateId.kVcBackgroundArt:
        return 'VcBackgroundArt';
      case SeaPenTemplateId.kVcBackgroundDreamscapesVc:
        return 'VcBackgroundDreamscapes';
      case SeaPenTemplateId.kVcBackgroundCharacters:
        return 'VcBackgroundCharacters';

      case 'Query':
        return 'Query';
    }
  }

  private onSelectedFeedbackChanged_(event:
                                         CustomEvent<{isThumbsUp: boolean}>) {
    const isThumbsUp = event.detail.isThumbsUp;
    const templateName = this.getTemplateNameFromId_(this.templateId);
    logSeaPenTemplateFeedback(templateName, isThumbsUp);
    const metadata = {
      isPositive: isThumbsUp,
      logId: templateName,
    };
    openFeedbackDialog(metadata, getSeaPenProvider());
  }
}

customElements.define(SeaPenImagesElement.is, SeaPenImagesElement);
