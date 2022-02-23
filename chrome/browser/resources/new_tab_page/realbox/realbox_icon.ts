// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AutocompleteMatch} from '../realbox.mojom-webui.js';

import {getTemplate} from './realbox_icon.html.js';

const DOCUMENT_MATCH_TYPE: string = 'document';

export type AutocompleteMatchWithImageData =
    AutocompleteMatch&{faviconDataUrl?: string, imageDataUrl?: string};

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
        computed: `computeBackgroundImage_(match.faviconDataUrl, match)`,
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
        computed: `computeImageSrc_(match.imageDataUrl, match)`,
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

  //============================================================================
  // Helpers
  //============================================================================

  private computeBackgroundImage_(): string {
    // If the match is a navigation one and has a favicon loaded, display that
    // as background image. Otherwise, display the colored SVG icon for
    // 'document' matches.
    // If 'google_g' is the default icon, display that as background image when
    // there is no match or the match is not a navigation one. Otherwise, don't
    // use a background image (use a mask image instead).
    if (this.match && !this.match.isSearchType) {
      if ((this.match as AutocompleteMatchWithImageData).faviconDataUrl) {
        return (this.match as AutocompleteMatchWithImageData).faviconDataUrl!;
      } else if (this.match.type === DOCUMENT_MATCH_TYPE) {
        return this.match.iconUrl;
      } else {
        return '';
      }
    } else if (this.defaultIcon === 'realbox/icons/google_g.svg') {
      return this.defaultIcon;
    } else {
      return '';
    }
  }

  private computeIsAnswer_(): boolean {
    return this.match && !!this.match.answer;
  }

  private computeMaskImage_(): string {
    if (this.match && (!this.match.isRichSuggestion || !this.inSearchbox)) {
      return this.match.iconUrl;
    } else {
      return this.defaultIcon;
    }
  }

  private computeIconStyle_(): string {
    // Use a background image if applicabale. Otherwise use a mask image.
    if (this.backgroundImage) {
      return `background-image: url(${this.backgroundImage});` +
          `background-color: transparent;`;
    } else {
      return `-webkit-mask-image: url(${this.maskImage});`;
    }
  }

  private computeImageSrc_(): string {
    if (!this.match) {
      return '';
    }

    if ((this.match as AutocompleteMatchWithImageData).imageDataUrl) {
      return (this.match as AutocompleteMatchWithImageData).imageDataUrl!;
    } else if (
        this.match.imageUrl && this.match.imageUrl.startsWith('data:image/')) {
      // zero-prefix matches come with the image content in |match.imageUrl|.
      return this.match.imageUrl;
    } else {
      return '';
    }
  }

  private containerBgColor_(imageSrc: string, imageDominantColor: string):
      string {
    // If the match has an image dominant color, show that color in place of the
    // image until it loads. This helps the image appear to load more smoothly.
    return (!imageSrc && imageDominantColor) ?
        // .25 opacity matching c/b/u/views/omnibox/omnibox_match_cell_view.cc.
        `${imageDominantColor}40` :
        '';
  }
}

customElements.define(RealboxIconElement.is, RealboxIconElement);
