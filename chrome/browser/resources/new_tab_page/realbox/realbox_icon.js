// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

const DOCUMENT_MATCH_TYPE = 'document';

// The LHS icon. Used on autocomplete matches as well as the realbox input to
// render icons, favicons, and entity images.
class RealboxIconElement extends PolymerElement {
  static get is() {
    return 'ntp-realbox-icon';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      //========================================================================
      // Public properties
      //========================================================================

      /**
       * Used as a background image on #icon if non-empty.
       * @type {string}
       */
      backgroundImage: {
        type: String,
        computed: `computeBackgroundImage_(match.faviconDataUrl, match)`,
        reflectToAttribute: true,
      },

      /**
       * The default icon to show when no match is selected and/or for
       * non-navigation matches. Only set in the context of the realbox input.
       * @type {string}
       */
      defaultIcon: {
        type: String,
        value: '',
      },

      /**
       * Whether icon is in searchbox or not. Used to prevent
       * the match icon of rich suggestions from showing in the context of the
       * realbox input.
       * @type {boolean}
       */
      inSearchbox: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      /**
       * Used as a mask image on #icon if |backgroundImage| is empty.
       * @type {string}
       */
      maskImage: {
        type: String,
        computed: `computeMaskImage_(match)`,
        reflectToAttribute: true,
      },

      /**
       * @type {!realbox.mojom.AutocompleteMatch}
       */
      match: {
        type: Object,
      },

      //========================================================================
      // Private properties
      //========================================================================

      /**
       * @type {string}
       * @private
       */
      iconStyle_: {
        type: String,
        computed: `computeIconStyle_(backgroundImage, maskImage)`,
      },

      /**
       * @type {string}
       * @private
       */
      imageSrc_: {
        type: String,
        computed: `computeImageSrc_(match.imageDataUrl, match)`,
      },
    };
  }

  //============================================================================
  // Helpers
  //============================================================================

  /**
   * @returns {string}
   * @private
   * @suppress {checkTypes}
   */
  computeBackgroundImage_() {
    // If the match is a navigation one and has a favicon loaded, display that
    // as background image. Otherwise, display the colored SVG icon for
    // 'document' matches.
    // If 'google_g' is the default icon, display that as background image when
    // there is no match or the match is not a navigation one. Otherwise, don't
    // use a background image (use a mask image instead).
    if (this.match && !this.match.isSearchType) {
      if (this.match.faviconDataUrl) {
        return this.match.faviconDataUrl;
      } else if (this.match.type === DOCUMENT_MATCH_TYPE) {
        return this.match.iconUrl;
      } else {
        return '';
      }
    } else if (this.defaultIcon === 'google_g.png') {
      return this.defaultIcon;
    } else {
      return '';
    }
  }

  /**
   * @returns {string}
   * @private
   */
  computeMaskImage_() {
    if (this.match && (!this.match.isRichSuggestion || !this.inSearchbox)) {
      return this.match.iconUrl;
    } else {
      return this.defaultIcon;
    }
  }

  /**
   * @returns {string}
   * @private
   */
  computeIconStyle_() {
    // Use a background image if applicabale. Otherwise use a mask image.
    if (this.backgroundImage) {
      return `background-image: url(${this.backgroundImage});` +
          `background-color: transparent;`;
    } else {
      return `-webkit-mask-image: url(${this.maskImage});`;
    }
  }

  /**
   * @returns {string}
   * @private
   * @suppress {checkTypes}
   */
  computeImageSrc_() {
    if (!this.match) {
      return '';
    }

    if (this.match.imageDataUrl) {
      return this.match.imageDataUrl;
    } else if (
        this.match.imageUrl && this.match.imageUrl.startsWith('data:image/')) {
      // zero-prefix matches come with the image content in |match.imageUrl|.
      return this.match.imageUrl;
    } else {
      return '';
    }
  }

  /**
   * @param {string} imageSrc
   * @param {string} imageDominantColor
   * @returns {string}
   * @private
   */
  containerBgColor_(imageSrc, imageDominantColor) {
    // If the match has an image dominant color, show that color in place of the
    // image until it loads. This helps the image appear to load more smoothly.
    return (!imageSrc && imageDominantColor) ?
        // .25 opacity matching c/b/u/views/omnibox/omnibox_match_cell_view.cc.
        `${imageDominantColor}40` :
        '';
  }
}

customElements.define(RealboxIconElement.is, RealboxIconElement);
