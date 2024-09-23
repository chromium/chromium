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
import 'chrome://resources/ash/common/sea_pen/sea_pen_icons.html.js';
import 'chrome://resources/ash/common/sea_pen/surface_effects/sparkle_placeholder.js';
import 'chrome://resources/ash/common/cr_elements/cr_auto_img/cr_auto_img.js';
import 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import './sea_pen_error_element.js';
import './sea_pen_feedback_element.js';
import './sea_pen_image_loading_element.js';
import './sea_pen_zero_state_svg_element.js';

import {afterNextRender} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {QUERY, Query, SeaPenImageId} from './constants.js';
import {isLacrosEnabled, isManagedSeaPenFeedbackEnabled, isSeaPenTextInputEnabled, isVcResizeThumbnailEnabled} from './load_time_booleans.js';
import {MantaStatusCode, SeaPenQuery, SeaPenThumbnail, TextQueryHistoryEntry} from './sea_pen.mojom-webui.js';
import {clearSeaPenThumbnails, openFeedbackDialog, selectSeaPenThumbnail} from './sea_pen_controller.js';
import {SeaPenTemplateId} from './sea_pen_generated.mojom-webui.js';
import {getTemplate} from './sea_pen_images_element.html.js';
import {getSeaPenProvider} from './sea_pen_interface_provider.js';
import {logSeaPenTemplateFeedback, logSeaPenThumbnailClicked} from './sea_pen_metrics_logger.js';
import {WithSeaPenStore} from './sea_pen_store.js';
import {isNonEmptyArray, isPersonalizationApp, isSeaPenImageId} from './sea_pen_utils.js';

const kFreeformLoadingPlaceholderCount = 4;
const kTemplateLoadingPlaceholderCount = 8;

export class SeaPenHistoryPromptSelectedEvent extends CustomEvent<string> {
  static readonly EVENT_NAME = 'sea-pen-history-prompt-selected';

  constructor(prompt: string) {
    super(
        SeaPenHistoryPromptSelectedEvent.EVENT_NAME,
        {
          bubbles: true,
          composed: true,
          detail: prompt,
        },
    );
  }
}

declare global {
  interface HTMLElementEventMap {
    [SeaPenHistoryPromptSelectedEvent.EVENT_NAME]:
        SeaPenHistoryPromptSelectedEvent;
  }
}

type Tile = 'loading'|SeaPenThumbnail;

let cameraAspectRatio: number|null = null;
(function() {
// Try to set aspect ratio if it is not set yet.
// We only need this when it is not Wallpaper, not Lacros, and aspectRatio
// is not set.
if (!isPersonalizationApp() && !isLacrosEnabled() &&
    isVcResizeThumbnailEnabled()) {
  if (navigator.mediaDevices.getUserMedia) {
    navigator.mediaDevices.getUserMedia({video: true})
        .then((stream: MediaStream) => {
          const videoTracks = stream.getVideoTracks();
          if (videoTracks.length > 0) {
            cameraAspectRatio =
                stream.getVideoTracks()[0]?.getSettings()?.aspectRatio ?? null;
          }
          // Stop all tracks.
          stream.getTracks().forEach(track => track.stop());
        })
        .catch((err) => {
          console.log(err);
        });
  }
}
})();

// This function resets the img.style.width and img.style.height so that the img
// can be perfectly aligned with the camera.
function calculateAndSetAspectRatio(img: HTMLImageElement) {
  const imgAspectRatio: number = img.naturalWidth / img.naturalHeight;

  if (imgAspectRatio > cameraAspectRatio!) {
    // Larger imgAspectRatio means the image is too wide for the camera, thus,
    // we keep the height as 100% and set the width as more than 100%. This
    // will crop the left and right side of the image and thus align with the
    // camera.
    img.style.width =
        ((imgAspectRatio / cameraAspectRatio!) * 100).toFixed(4) + '%';
    img.style.height = '100%';
  } else {
    // Smaller imgAspectRatio means the image is too tall for the camera,
    // thus, we keep the width as 100% and set the height as more than 100%.
    // This will crop the top and bottom side of the image and thus align with
    // the camera.
    img.style.height =
        ((cameraAspectRatio! / imgAspectRatio) * 100).toFixed(4) + '%';
    img.style.width = '100%';
  }
}

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

      thumbnails_: {
        type: Object,
        observer: 'onThumbnailsChanged_',
      },

      thumbnailsLoading_: {
        type: Boolean,
        observer: 'onThumbnailsLoadingChanged_',
      },

      /**
       * List of tiles to be displayed to the user. Updated when `thumbnails_`
       * or `thumbnailsLoading_` changed.
       */
      tiles_: {
        type: Array,
        value() {
          // Pre-populate the tiles with placeholders.
          return new Array(kTemplateLoadingPlaceholderCount).fill('loading');
        },
      },

      currentSelected_: {
        type: Number,
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

      isSeaPenTextInputEnabled_: {
        type: Boolean,
        value() {
          return isSeaPenTextInputEnabled();
        },
      },

      isManagedSeaPenFeedbackEnabled_: {
        type: Boolean,
        value() {
          return isManagedSeaPenFeedbackEnabled();
        },
      },

      showHistory_: {
        type: Boolean,
        computed:
            'computeShowHistory_(thumbnailsLoading_, seaPenQuery_, textQueryHistory_)',
      },

      seaPenQuery_: {
        type: Object,
        value: null,
      },

      textQueryHistory_: {
        type: Array,
        value: null,
      },
    };
  }

  private templateId: SeaPenTemplateId|Query;
  private thumbnails_: SeaPenThumbnail[]|null;
  private thumbnailsLoading_: boolean;
  private tiles_: Tile[];
  private currentSelected_: SeaPenImageId|null;
  private pendingSelected_: SeaPenImageId|SeaPenThumbnail|null;
  private thumbnailResponseStatusCode_: MantaStatusCode|null;
  private showError_: boolean;
  private cameraFeed_: HTMLVideoElement|null;
  private isSeaPenTextInputEnabled_: boolean;
  private seaPenQuery_: SeaPenQuery|null;
  private textQueryHistory_: TextQueryHistoryEntry[]|null;

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
    this.watch<SeaPenImagesElement['seaPenQuery_']>(
        'seaPenQuery_', state => state.currentSeaPenQuery);
    this.watch<SeaPenImagesElement['textQueryHistory_']>(
        'textQueryHistory_', state => state.textQueryHistory);
    this.updateFromStore();
  }

  private computeShowError_(
      statusCode: MantaStatusCode|null, thumbnailsLoading: boolean): boolean {
    return !!statusCode && !thumbnailsLoading;
  }

  private getPoweredByGoogleMessage_(): string {
    return isPersonalizationApp() ?
        this.i18n('seaPenWallpaperPoweredByGoogle') :
        this.i18n('vcBackgroundPoweredByGoogle');
  }

  private onTemplateIdChanged_() {
    this.cameraFeed_?.remove();
    this.cameraFeed_ = null;
    if (this.templateId === QUERY) {
      return;
    }
    // Clear thumbnails if changing templates.
    // For Freeform, we need to preserve the thumbnails state when switching
    // between freeform tabs.
    clearSeaPenThumbnails(this.getStore());
  }

  private shouldShowZeroState_(
      thumbnailsLoading: boolean, thumbnails: SeaPenThumbnail[]|null): boolean {
    return !thumbnails && !thumbnailsLoading;
  }

  private isSeaPenThumbnail_(item: Tile|null|
                             undefined): item is SeaPenThumbnail {
    return !!item && typeof item === 'object' && 'id' in item &&
        typeof item.id === 'number';
  }

  private shouldShowImageThumbnails_(
      thumbnailsLoading: boolean, thumbnails: SeaPenThumbnail[]|null): boolean {
    return thumbnailsLoading || isNonEmptyArray(thumbnails);
  }

  private shouldShowImagesHeading_(
      isSeaPenTextInputEnabled: boolean, templateId: SeaPenTemplateId|Query) {
    return !isSeaPenTextInputEnabled || templateId !== QUERY;
  }

  private shouldShowThumbnailFeedback_(
      isManagedSeaPenFeedbackEnabled: boolean, thumbnailsLoading: boolean) {
    return isManagedSeaPenFeedbackEnabled && !thumbnailsLoading;
  }

  private getPlaceholders_(x: number) {
    return new Array(x).fill(0);
  }

  private isTileVisible_(tile: Tile|null|undefined, thumbnailsLoading: boolean):
      boolean {
    if (thumbnailsLoading) {
      return false;
    }
    return this.isSeaPenThumbnail_(tile);
  }

  private onThumbnailsChanged_(thumbnails: SeaPenThumbnail[]) {
    if (!isNonEmptyArray(thumbnails)) {
      return;
    }

    this.updateList(
        /*propertyPath=*/ 'tiles_',
        /*identityGetter=*/
        (tile: Tile) => {
          if (this.isSeaPenThumbnail_(tile)) {
            return tile.id.toString();
          }
          return tile;
        },
        /*newList=*/ thumbnails,
        /*identityBasedUpdate=*/ true,
    );

    if (this.cameraFeed_) {
      this.cameraFeed_.style.display = 'none';
    }

    // focus on the first thumbnail if the thumbnails are generated
    // successfully.
    afterNextRender(this, () => {
      window.scrollTo(0, 0);
      this.shadowRoot!.querySelector<HTMLElement>('.sea-pen-image')?.focus();

      // Resize images if cameraAspectRatio is set.
      // This only happens when it is not wallpaper, not lacros.
      if (cameraAspectRatio) {
        // Handle each sea-pen-image element.
        this.shadowRoot!.querySelectorAll<HTMLElement>('.sea-pen-image')
            .forEach((gridItem: HTMLElement) => {
              const img: HTMLImageElement =
                  gridItem.shadowRoot!.querySelector<HTMLImageElement>('img')!;

              if (img.complete) {
                calculateAndSetAspectRatio(img);
              } else {
                img.onload = () => calculateAndSetAspectRatio(img);
              }
            });
      }
    });
  }

  private onThumbnailsLoadingChanged_(thumbnailsLoading: boolean) {
    if (!thumbnailsLoading) {
      return;
    }

    const placeholderCount = this.templateId === QUERY ?
        kFreeformLoadingPlaceholderCount :
        kTemplateLoadingPlaceholderCount;

    this.updateList(
        /*propertyPath=*/ 'tiles_',
        /*identityGetter=*/
        () => 'loading',
        /*newList=*/ new Array(placeholderCount).fill('loading'),
        /*identityBasedUpdate=*/ false,
    );
  }

  private maybeCreateCameraFeed_(): HTMLVideoElement|null {
    if (isPersonalizationApp() || isLacrosEnabled()) {
      return null;
    }
    let cameraFeed: HTMLVideoElement|null = document.createElement('video');
    // Stretch camera stream to fit into the image.
    cameraFeed.style.objectFit = 'cover';
    // Align camera feed with the clicked image.
    cameraFeed.style.position = 'relative';
    // Flip left and right so that camera matches with the image.
    cameraFeed.style.transform = 'scale(-1, 1)';

    if (navigator.mediaDevices.getUserMedia) {
      navigator.mediaDevices.getUserMedia({video: true})
          .then(function(stream: MediaStream) {
            cameraFeed!.srcObject = stream;
            cameraFeed!.play();
          })
          .catch(function(err) {
            console.log(err);
            cameraFeed = null;
          });
    }

    return cameraFeed;
  }

  private onThumbnailSelected_(event: Event&{model: {item: Tile}}) {
    if (!this.isSeaPenThumbnail_(event.model.item)) {
      return;
    }

    this.cameraFeed_?.remove();
    this.cameraFeed_ = this.maybeCreateCameraFeed_();

    if (this.cameraFeed_) {
      // Attached cameraFeed_ to the selected image.
      const item = ((event.target as Element)!.shadowRoot as
                    ShadowRoot)!.querySelector<HTMLElement>('.item')!;
      this.cameraFeed_.remove();
      item.appendChild(this.cameraFeed_);
      this.cameraFeed_.width = item.clientWidth;
      this.cameraFeed_.height = item.clientHeight;
      this.cameraFeed_.style.display = 'block';
    }

    logSeaPenThumbnailClicked(this.templateId);
    selectSeaPenThumbnail(
        event.model.item, getSeaPenProvider(), this.getStore());
  }

  private getAriaIndex_(i: number): number {
    return i + 1;
  }

  private getAriaDescription_(
      thumbnail: Tile|undefined, currentSelected: SeaPenImageId|null,
      pendingSelected: SeaPenImageId|SeaPenThumbnail|null): string {
    // TODO(b/331657978): update the real string for aria-description of Sea Pen
    // image.
    if (this.isThumbnailPendingSelected_(thumbnail, pendingSelected)) {
      // Do not show upscaling message for Vc Background.
      return isPersonalizationApp() ? this.i18n('seaPenCreatingHighResImage') :
                                      '';
    }
    if (this.isThumbnailSelected_(
            thumbnail, currentSelected, pendingSelected)) {
      return isPersonalizationApp() ? this.i18n('seaPenSetWallpaper') :
                                      this.i18n('seaPenSetCameraBackground');
    }
    return '';
  }

  private isThumbnailSelected_(
      thumbnail: Tile|undefined, currentSelected: SeaPenImageId|null,
      pendingSelected: SeaPenImageId|SeaPenThumbnail|null): boolean {
    if (!thumbnail || thumbnail === 'loading') {
      return false;
    }

    // Image was just clicked on and is currently being set.
    if (thumbnail === pendingSelected) {
      return true;
    }

    // Image was previously selected, and was just clicked again via the "Recent
    // Images" section. This can arise if the user quickly navigates back and
    // forth from SeaPen root and results page while selecting images.
    if (isSeaPenImageId(pendingSelected)) {
      return thumbnail.id === pendingSelected;
    }

    // No pending image in progress. Currently selected image matches the
    // thumbnail id.
    return pendingSelected === null && currentSelected === thumbnail.id;
  }

  private isThumbnailPendingSelected_(
      thumbnail: Tile|undefined,
      pendingSelected: SeaPenImageId|SeaPenThumbnail|null): boolean {
    return this.isSeaPenThumbnail_(thumbnail) && !!thumbnail &&
        thumbnail === pendingSelected;
  }

  // START AUTOGENERATED - DO NOT EDIT!
  // Get the name of the template for metrics. Must match histograms.xml
  // SeaPenTemplateName.
  private getTemplateNameFromId_(templateId: SeaPenTemplateId|Query): string {
    switch (templateId) {
      case SeaPenTemplateId.kFlower:
        return 'Flower';
      case SeaPenTemplateId.kMineral:
        return 'Mineral';
      case SeaPenTemplateId.kArt:
        return 'Art';
      case SeaPenTemplateId.kCharacters:
        return 'Characters';
      case SeaPenTemplateId.kTerrain:
        return 'Terrain';
      case SeaPenTemplateId.kCurious:
        return 'Curious';
      case SeaPenTemplateId.kDreamscapes:
        return 'Dreamscapes';
      case SeaPenTemplateId.kTranslucent:
        return 'Translucent';
      case SeaPenTemplateId.kScifi:
        return 'Scifi';
      case SeaPenTemplateId.kLetters:
        return 'Letters';
      case SeaPenTemplateId.kGlowscapes:
        return 'Glowscapes';
      case SeaPenTemplateId.kSurreal:
        return 'Surreal';
      case SeaPenTemplateId.kTerrainAlternate:
        return 'TerrainAlternate';

      case SeaPenTemplateId.kVcBackgroundSimple:
        return 'VcBackgroundSimple';
      case SeaPenTemplateId.kVcBackgroundOffice:
        return 'VcBackgroundOffice';
      case SeaPenTemplateId.kVcBackgroundTerrainVc:
        return 'VcBackgroundTerrainVc';
      case SeaPenTemplateId.kVcBackgroundCafe:
        return 'VcBackgroundCafe';
      case SeaPenTemplateId.kVcBackgroundArt:
        return 'VcBackgroundArt';
      case SeaPenTemplateId.kVcBackgroundDreamscapesVc:
        return 'VcBackgroundDreamscapesVc';
      case SeaPenTemplateId.kVcBackgroundCharacters:
        return 'VcBackgroundCharacters';
      case SeaPenTemplateId.kVcBackgroundGlowscapes:
        return 'VcBackgroundGlowscapes';
      case QUERY:
        return isPersonalizationApp() ? 'Freeform' : 'VcBackgroundFreeform';
    }
  }
  // END AUTOGENERATED - DO NOT EDIT!

  private onSelectedFeedbackChanged_(
      event: CustomEvent<{isThumbsUp: boolean, thumbnailId: number}>) {
    const isThumbsUp = event.detail.isThumbsUp;
    const templateName = this.getTemplateNameFromId_(this.templateId);
    logSeaPenTemplateFeedback(templateName, isThumbsUp);
    const metadata = {
      isPositive: isThumbsUp,
      logId: templateName,
      generationSeed: event.detail.thumbnailId,
    };
    openFeedbackDialog(metadata, getSeaPenProvider());
  }

  private computeShowHistory_(
      thumbnailsLoading: boolean, seaPenQuery: SeaPenQuery|null,
      textQueryHistory: TextQueryHistoryEntry[]): boolean {
    return !thumbnailsLoading && !!seaPenQuery?.textQuery &&
        isNonEmptyArray(textQueryHistory);
  }

  private onHistoryPromptClicked_(e: Event&
                                  {model: {item: TextQueryHistoryEntry}}) {
    this.dispatchEvent(
        new SeaPenHistoryPromptSelectedEvent(e.model.item.query));
  }
}

customElements.define(SeaPenImagesElement.is, SeaPenImagesElement);
