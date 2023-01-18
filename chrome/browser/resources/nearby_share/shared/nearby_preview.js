// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'nearby-preview' component shows a preview of data to be
 * sent to a remote device. The data might be some plain text, a URL or a file.
 */

import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './nearby_shared_icons.html.js';
import './nearby_shared_share_type_icons.html.js';

import {PayloadPreview} from '/mojo/nearby_share.mojom-webui.js';
import {ShareType} from '/mojo/nearby_share_share_type.mojom-webui.js';
import {assertNotReached} from 'chrome://resources/ash/common/assert.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './nearby_preview.html.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const NearbyPreviewElementBase = mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class NearbyPreviewElement extends NearbyPreviewElementBase {
  static get is() {
    return 'nearby-preview';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Preview info for the file(s) to send. Expected to start
       * as null, then change to a valid object before this component is shown.
       * @type {?PayloadPreview}
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
    };
  }

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
  }

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
      case ShareType.kUnknownFile:
        return 'nearbysharetype68:unknown-file';
      case ShareType.kMultipleFiles:
        return 'nearbysharetype68:multiple-file';
      case ShareType.kImageFile:
      case ShareType.kVideoFile:
        return 'nearbysharetype68:image-video-file';
      case ShareType.kAudioFile:
        return 'nearbysharetype68:audio-file';
      case ShareType.kPdfFile:
        return 'nearbysharetype68:pdf-file';
      case ShareType.kGoogleDocsFile:
        return 'nearbysharetype68:google-docs-file';
      case ShareType.kGoogleSheetsFile:
        return 'nearbysharetype68:google-sheets-file';
      case ShareType.kGoogleSlidesFile:
        return 'nearbysharetype68:google-slides-file';
      case ShareType.kText:
        return 'nearbysharetype68:text';
      case ShareType.kUrl:
        return 'nearbysharetype68:url';
      case ShareType.kAddress:
        return 'nearbysharetype68:address';
      case ShareType.kPhone:
        return 'nearbysharetype68:phone';
      case ShareType.kWifiCredentials:
        return 'nearbysharetype68:wifi-credentials';
      default:
        assertNotReached(
            'No icon defined for share type ' + this.payloadPreview.shareType);
        return 'nearbysharetype68:unknown-file';
    }
  }

  /**
   * @return {string} The css class to be applied to the icon.
   * @private
   */
  getIconClass_() {
    if (this.disabled) {
      return 'disabled';
    }
    return '';
  }
}

customElements.define(NearbyPreviewElement.is, NearbyPreviewElement);
