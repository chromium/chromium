// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import './shared_style.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MultidevicePhoneHubBrowserProxy} from './multidevice_phonehub_browser_proxy.js';
import {CameraRollManager} from './types.js';

Polymer({
  is: 'camera-roll-manager-form',

  _template: html`{__html_template__}`,

  properties: {
    /** @private */
    numberOfThumbnails_: {
      type: Number,
      value: 4,
    },
  },

  /** @private {?MultidevicePhoneHubBrowserProxy}*/
  browserProxy_: null,

  /** @override */
  created() {
    this.browserProxy_ = MultidevicePhoneHubBrowserProxy.getInstance();
  },

  /** @private */
  onNumberOfThumbnailsChanged_() {
    const inputValue = this.$$('#numberOfThumbnailsInput').value;
    if (inputValue > 16) {
      this.numberOfThumbnails_ = 16;
      return;
    }

    if (inputValue < 0) {
      this.numberOfThumbnails_ = 0;
      return;
    }

    this.numberOfThumbnails_ = Number(inputValue);
  },

  /** @private */
  setFakeCameraRollManager_() {
    const cameraRollManager = {
      numberOfThumbnails: this.numberOfThumbnails_,
    };
    this.browserProxy_.setCameraRoll(cameraRollManager);
  },
});
