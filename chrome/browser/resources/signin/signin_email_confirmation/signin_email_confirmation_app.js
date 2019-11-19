// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.m.js';
import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.m.js';
import './signin_shared_css.js';
import './strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

Polymer({
  is: 'signin-email-confirmation-app',

  _template: html`{__html_template__}`,

  /** @override */
  ready: function() {
    const args = /** @type {{lastEmail: string, newEmail: string}} */
        (JSON.parse(chrome.getVariableValue('dialogArguments')));
    const {lastEmail, newEmail} = args;
    this.$.dialogTitle.textContent =
        loadTimeData.getStringF('signinEmailConfirmationTitle', lastEmail);
    this.$.createNewUserRadioButtonSubtitle.textContent =
        loadTimeData.getStringF(
            'signinEmailConfirmationCreateProfileButtonSubtitle', newEmail);
    this.$.startSyncRadioButtonSubtitle.textContent = loadTimeData.getStringF(
        'signinEmailConfirmationStartSyncButtonSubtitle', newEmail);

    document.addEventListener('keydown', this.onKeyDown_.bind(this));
  },

  onKeyDown_: function(e) {
    // If the currently focused element isn't something that performs an action
    // on "enter" being pressed and the user hits "enter", perform the default
    // action of the dialog, which is "OK".
    if (e.key == 'Enter' &&
        !/^(A|CR-BUTTON)$/.test(getDeepActiveElement().tagName)) {
      this.$.confirmButton.click();
      e.preventDefault();
    }
  },

  /** @private */
  onConfirm_: function() {
    const action = this.$$('cr-radio-group').selected;
    chrome.send('dialogClose', [JSON.stringify({'action': action})]);
  },

  /** @private */
  onCancel_: function() {
    chrome.send('dialogClose', [JSON.stringify({'action': 'cancel'})]);
  },
});
