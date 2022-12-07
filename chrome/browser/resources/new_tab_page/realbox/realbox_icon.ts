// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getFaviconForPageURL} from 'chrome://resources/js/icon.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AutocompleteMatch} from '../omnibox.mojom-webui.js';

import {getTemplate} from './realbox_icon.html.js';

const DOCUMENT_MATCH_TYPE: string = 'document';
const HISTORY_CLUSTER_MATCH_TYPE: string = 'history-cluster';

export interface RealboxIconElement {
  $: {
    container: HTMLElement,
    icon: HTMLElement,
    image: HTMLImageElement,
  };
}

// The LHS icon. Used on autocomplete matches as well as the realbox input to
// render icons, favicons, and entity images.
export class RealboxIconElement extends PolymerElement {
  static get is() {
    return 'ntp-realbox-icon';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      //========================================================================
      // Public properties
      //========================================================================

      /** Used as a background image on #icon if non-empty. */
      backgroundImage: {
        type: String,
        computed: `computeBackgroundImage_(match.*)`,
        reflectToAttribute: true,
      },

      /**
       * The default icon to show when no match is selected and/or for
       * non-navigation matches. Only set in the context of the realbox input.
       */
      defaultIcon: {
        type: String,
        value: '',
      },

      /**
       * Whether icon is in searchbox or not. Used to prevent
       * the match icon of rich suggestions from showing in the context of the
       * realbox input.
       */
      inSearchbox: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      /**
       * Whether icon belongs to an answer or not. Used to prevent
       * the match image from taking size of container.
       */
      isAnswer: {
        type: Boolean,
        computed: `computeIsAnswer_(match)`,
        reflectToAttribute: true,
      },

      /** Used as a mask image on #icon if |backgroundImage| is empty. */
      maskImage: {
        type: String,
        computed: `computeMaskImage_(match)`,
        reflectToAttribute: true,
      },

      match: {
        type: Object,
      },

      //========================================================================
      // Private properties
      //========================================================================

      iconStyle_: {
        type: String,
        computed: `computeIconStyle_(backgroundImage, maskImage)`,
      },

      imageSrc_: {
        type: String,
        computed: `computeImageSrc_(match.imageUrl, match)`,
        observer: 'onImageSrcChanged_',
      },

      /**
       * Flag indicating whether or not an image is loading. This is used to
       * show a placeholder color while the image is loading.
       */
      imageLoading_: {
        type: Boolean,
        value: false,
      },
    };
  }

  backgroundImage: string;
  defaultIcon: string;
  inSearchbox: boolean;
  isAnswer: boolean;
  maskImage: string;
  match: AutocompleteMatch;
  private iconStyle_: string;
  private imageSrc_: string;
  private imageLoading_: boolean;

  //============================================================================
  // Helpers
  //============================================================================

  private computeBackgroundImage_(): string {
    if (this.match && !this.match.isSearchType) {
      if (this.match.type !== DOCUMENT_MATCH_TYPE &&
          this.match.type !== HISTORY_CLUSTER_MATCH_TYPE) {
        return getFaviconForPageURL(
            this.match.destinationUrl.url, /* isSyncedUrlForHistoryUi= */ false,
            /* remoteIconUrlForUma= */ '', /* size= */ 32,
            /* forceLightMode= */ true);
      }

      if (this.match.type === DOCUMENT_MATCH_TYPE) {
        return `url(${this.match.iconUrl})`;
      }
    }

    if (this.defaultIcon === 'realbox/icons/google_g.svg') {
      // The google_g.svg is a fully colored icon, so it needs to be displayed
      // as a background image as mask images will mask the colors.
      return `url(${this.defaultIcon})`;
    }

    return '';
  }

  private computeIsAnswer_(): boolean {
    return this.match && !!this.match.answer;
  }

  private computeMaskImage_(): string {
    if (this.match && (!this.match.isRichSuggestion || !this.inSearchbox)) {
      return `url(${this.match.iconUrl})`;
    } else {
      return `url(${this.defaultIcon})`;
    }
  }

  private computeIconStyle_(): string {
    // Use a background image if applicable. Otherwise use a mask image.
    if (this.backgroundImage) {
      return `background-image: ${this.backgroundImage};` +
          `background-color: transparent;`;
    } else {
      return `-webkit-mask-image: ${this.maskImage};`;
    }
  }

  private computeImageSrc_(): string {
    if (!this.match || !this.match.imageUrl) {
      return '';
    }

    if (this.match.imageUrl.startsWith('data:image/')) {
      // Zero-prefix matches come with the image content in |match.imageUrl|.
      return this.match.imageUrl;
    }

    return `chrome://image?${this.match.imageUrl}`;
  }

  private containerBgColor_(imageDominantColor: string, imageLoading: boolean):
      string {
    // If the match has an image dominant color, show that color in place of the
    // image until it loads. This helps the image appear to load more smoothly.
    return (imageLoading && imageDominantColor) ?
        // .25 opacity matching c/b/u/views/omnibox/omnibox_match_cell_view.cc.
        `${imageDominantColor}40` :
        'transparent';
  }

  private onImageSrcChanged_() {
    // If imageSrc_ changes to a new truthy value, a new image is being loaded.
    this.imageLoading_ = !!this.imageSrc_;
  }

  private onImageLoad_() {
    this.imageLoading_ = false;
  }
}

customElements.define(RealboxIconElement.is, RealboxIconElement);
