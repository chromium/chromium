// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/bluetooth/bluetooth_pairing_ui.js';
import 'chrome://resources/cr_elements/chromeos/cros_color_overrides.css.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/cr_page_host_style.css.js';
import './strings.m.js';

import {I18nBehavior} from 'chrome://resources/ash/common/i18n_behavior.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {startColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview
 * 'bluetooth-dialog-host' is used to host a <bluetooth-pairing-ui> element to
 * manage bluetooth pairing. The dialog arguments are provided in the
 * chrome 'dialogArguments' variable.
 */

Polymer({
  is: 'bluetooth-pairing-dialog',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  properties: {
    /**
     * Current Pairing device.
     * @type {!chrome.bluetooth.Device|undefined}
     * @private
     */
    pairingDevice_: Object,

    /**
     * The address, when set, of the specific device that will be attempted to
     * be paired with by the pairing dialog. If null, no specific device will be
     * paired with and the user will be allowed to select a device to pair with.
     * @private {?string}
     */
    deviceAddress_: {
      type: String,
      value: null,
    },

    /**
     * Flag indicating whether links should be displayed or not. In some
     * cases, such as the user being in OOBE or the login screen, links will
     * not work and should not be displayed.
     */
    shouldOmitLinks_: {
      type: Boolean,
      value: false,
    },

    /**
     * Whether the Jelly feature flag is enabled.
     * @private
     */
    isJellyEnabled_: {
      type: Boolean,
      readOnly: true,
      value() {
        return loadTimeData.valueExists('isJellyEnabled') &&
            loadTimeData.getBoolean('isJellyEnabled');
      },
    },
  },

  /** @override */
  attached() {
    if (this.isJellyEnabled_) {
      const link = document.createElement('link');
      link.rel = 'stylesheet';
      link.href = 'chrome://theme/colors.css?sets=legacy,sys';
      document.head.appendChild(link);
      document.body.classList.add('jelly-enabled');
      startColorChangeUpdater();
    }

    const dialogArgs = chrome.getVariableValue('dialogArguments');
    if (!dialogArgs) {
      return;
    }

    const parsedDialogArgs = JSON.parse(dialogArgs);
    if (!parsedDialogArgs) {
      return;
    }

    this.deviceAddress_ = parsedDialogArgs.address;
    this.shouldOmitLinks_ = !!parsedDialogArgs.shouldOmitLinks;
  },

  /** @private */
  closeDialog_() {
    chrome.send('dialogClose');
  },
});
