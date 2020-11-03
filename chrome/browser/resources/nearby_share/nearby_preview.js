// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'nearby-preview' component shows a preview of data to be
 * sent to a remote device. The data might be some plain text, a URL or a file.
 */

import './shared/nearby_shared_share_type_icons.m.js';

import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

Polymer({
  is: 'nearby-preview',

  _template: html`{__html_template__}`,

  properties: {
    /** The title to show below the preview graphic. */
    title: {
      type: String,
      value: '',
    },

    /**
     * The type of the attachment. Determines the icon displayed.
     * @type {?nearbyShare.mojom.ShareType}
     * */
    shareType: {
      type: Number,
      value: null,
    }
  },

  /**
   * @return {string} the identifier for the iron icon
   * @private
   */
  getIronIconName_() {
    if (this.shareType === null || this.shareType === undefined) {
      return '';
    }

    switch (this.shareType) {
      case nearbyShare.mojom.ShareType.kUnknownFile:
        return 'nearbysharetype40:unknown-file';
      case nearbyShare.mojom.ShareType.kMultipleFiles:
        return 'nearbysharetype40:multiple-file';
      case nearbyShare.mojom.ShareType.kImageFile:
      case nearbyShare.mojom.ShareType.kVideoFile:
        return 'nearbysharetype40:image-video-file';
      case nearbyShare.mojom.ShareType.kAudioFile:
        return 'nearbysharetype40:audio-file';
      case nearbyShare.mojom.ShareType.kPdfFile:
        return 'nearbysharetype40:pdf-file';
      case nearbyShare.mojom.ShareType.kGoogleDocsFile:
        return 'nearbysharetype40:google-docs-file';
      case nearbyShare.mojom.ShareType.kGoogleSheetsFile:
        return 'nearbysharetype40:google-sheets-file';
      case nearbyShare.mojom.ShareType.kGoogleSlidesFile:
        return 'nearbysharetype40:google-slides-file';
      case nearbyShare.mojom.ShareType.kText:
        return 'nearbysharetype40:text';
      case nearbyShare.mojom.ShareType.kUrl:
        return 'nearbysharetype40:url';
      case nearbyShare.mojom.ShareType.kAddress:
        return 'nearbysharetype40:address';
      case nearbyShare.mojom.ShareType.kPhone:
        return 'nearbysharetype40:phone';
      default:
        assertNotReached('No icon defined for share type ' + this.shareType);
        return 'nearbysharetype40:unknown-file';
    }
  },
});
