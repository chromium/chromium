// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_drawer/cr_drawer.js';
import 'chrome://resources/ash/common/cr_elements/cr_toolbar/cr_toolbar.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';
import 'chrome://resources/polymer/v3_0/iron-pages/iron-pages.js';
import './audio_settings.js';
import './battery_settings.js';
import './bluetooth_settings.js';
import './icons.js';
import './input_device_settings.js';
import './shared_styles.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

Polymer({
  is: 'device-emulator-pages',

  _template: html`{__html_template__}`,

  properties: {
    selectedPage: {
      type: Number,
      value: 0,
      observer: 'onSelectedPageChange_',
    },
  },

  /** @override */
  ready() {
    chrome.send('initializeDeviceEmulator');
  },

  /** @private */
  onMenuButtonClick_() {
    this.$.drawer.toggle();
  },

  /** @private */
  onSelectedPageChange_() {
    this.$.drawer.close();
  },
});
