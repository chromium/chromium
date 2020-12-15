// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'nearby-preview' component shows a preview of data to be
 * sent to a remote device. The data might be some plain text, a URL or a file.
 */

Polymer({
  is: 'nearby-preview',

  behaviors: [I18nBehavior],

  properties: {
    /**
     * Preview info for the file(s) to send. Expected to start
     * as null, then change to a valid object before this component is shown.
     * @type {?nearbyShare.mojom.PayloadPreview}
     */
    payloadPreview: {
      type: Object,
      value: null,
    },

    /**
     * Controls whether the icon should be greyed out.
     * @type {boolean}
     */
    disabled: {
      type: Boolean,
      value: false,
    },
  },

  /**
   * @return {string} the preview text to display
   * @private
   */
  getTitle_() {
    if (!this.payloadPreview) {
      return '';
    }

    if (this.payloadPreview.fileCount && this.payloadPreview.fileCount > 1) {
      return this.i18n(
          'nearbySharePreviewMultipleFileTitle', this.payloadPreview.fileCount);
    } else if (this.payloadPreview.description) {
      return this.payloadPreview.description;
    } else {
      return '';
    }
  },

  /**
   * @return {string} the identifier for the iron icon
   * @private
   */
  getIronIconName_() {
    if (!this.payloadPreview || this.payloadPreview.shareType === null ||
        this.payloadPreview.shareType === undefined) {
      return '';
    }

    switch (this.payloadPreview.shareType) {
      case nearbyShare.mojom.ShareType.kUnknownFile:
        return 'nearbysharetype68:unknown-file';
      case nearbyShare.mojom.ShareType.kMultipleFiles:
        return 'nearbysharetype68:multiple-file';
      case nearbyShare.mojom.ShareType.kImageFile:
      case nearbyShare.mojom.ShareType.kVideoFile:
        return 'nearbysharetype68:image-video-file';
      case nearbyShare.mojom.ShareType.kAudioFile:
        return 'nearbysharetype68:audio-file';
      case nearbyShare.mojom.ShareType.kPdfFile:
        return 'nearbysharetype68:pdf-file';
      case nearbyShare.mojom.ShareType.kGoogleDocsFile:
        return 'nearbysharetype68:google-docs-file';
      case nearbyShare.mojom.ShareType.kGoogleSheetsFile:
        return 'nearbysharetype68:google-sheets-file';
      case nearbyShare.mojom.ShareType.kGoogleSlidesFile:
        return 'nearbysharetype68:google-slides-file';
      case nearbyShare.mojom.ShareType.kText:
        return 'nearbysharetype68:text';
      case nearbyShare.mojom.ShareType.kUrl:
        return 'nearbysharetype68:url';
      case nearbyShare.mojom.ShareType.kAddress:
        return 'nearbysharetype68:address';
      case nearbyShare.mojom.ShareType.kPhone:
        return 'nearbysharetype68:phone';
      default:
        assertNotReached(
            'No icon defined for share type ' + this.payloadPreview.shareType);
        return 'nearbysharetype68:unknown-file';
    }
  },

  /**
   * @return {string} The css class to be applied to the icon.
   * @private
   */
  getIconClass_() {
    if (this.disabled) {
      return 'disabled';
    }
    return '';
  },
});
