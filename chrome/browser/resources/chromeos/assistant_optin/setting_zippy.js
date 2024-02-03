// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';
import '../components/common_styles/oobe_common_styles.css.js';
import './assistant_common_styles.css.js';

import {html, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AssistantNativeIconType} from './utils.js';


/** @polymer */
class SettingZippy extends PolymerElement {
  static get is() {
    return 'setting-zippy';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      iconSrc: {
        type: String,
        value: null,
      },

      hideLine: {
        type: Boolean,
        value: false,
      },

      cardStyle: {
        type: Boolean,
        value: false,
      },

      nativeIconType: {
        type: Number,
        value: AssistantNativeIconType.NONE,
      },

      nativeIconLabel: {
        type: String,
        value: null,
      },
    };
  }

  /**
   * Wrap the icon as a image into a html snippet.
   *
   * @param {string} iconUri the icon uri to be wrapped.
   * @param {string} imageLabel the aria label of the image.
   * @param {string} background the background color of the icon, default to
   * white if unspecified.
   * @return {string} wrapped html snippet.
   *
   * @private
   */
  getWrappedIcon(iconUri, imageLabel, background) {
    return String.raw`
    <html>
      <style>
        body {
          margin: 0;
        }
        #icon {
          background: ` +
        (background || 'white') + `;
          width: 20px;
          height: 20px;
          user-select: none;
        }
      </style>
    <body><img id="icon" aria-label="` +
        imageLabel + `" src="` + iconUri + '"></body></html>';
  }

  shouldUseWebviewIcon_(iconSrc, nativeIconType) {
    return iconSrc !== null && nativeIconType === AssistantNativeIconType.NONE;
  }

  shouldUseWAANativeIcon_(nativeIconType) {
    return nativeIconType === AssistantNativeIconType.WAA;
  }

  shouldUseDANativeIcon_(nativeIconType) {
    return nativeIconType === AssistantNativeIconType.DA;
  }

  shouldUseInfoNativeIcon_(nativeIconType) {
    return nativeIconType === AssistantNativeIconType.INFO;
  }
}

customElements.define(SettingZippy.is, SettingZippy);
