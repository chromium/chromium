// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.js';
import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import './signin_shared.css.js';
import './strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './signin_email_confirmation_app.html.js';

interface SigninEmailConfirmationAppElement {
  $: {
    dialogTitle: HTMLElement,
    createNewUserRadioButtonSubtitle: HTMLElement,
    startSyncRadioButtonSubtitle: HTMLElement,
  };
}

class SigninEmailConfirmationAppElement extends PolymerElement {
  static get is() {
    return 'signin-email-confirmation-app';
  }

  static get template() {
    return getTemplate();
  }

  override ready() {
    super.ready();

    const args = JSON.parse(chrome.getVariableValue('dialogArguments'));
    const {lastEmail, newEmail} = args;
    this.$.dialogTitle.textContent =
        loadTimeData.getStringF('signinEmailConfirmationTitle', lastEmail);
    this.$.createNewUserRadioButtonSubtitle.textContent =
        loadTimeData.getStringF(
            'signinEmailConfirmationCreateProfileButtonSubtitle', newEmail);
    this.$.startSyncRadioButtonSubtitle.textContent = loadTimeData.getStringF(
        'signinEmailConfirmationStartSyncButtonSubtitle', newEmail);
  }

  private onConfirm_() {
    const action = this.shadowRoot!.querySelector('cr-radio-group')!.selected;
    chrome.send('dialogClose', [JSON.stringify({'action': action})]);
  }

  private onCancel_() {
    chrome.send('dialogClose', [JSON.stringify({'action': 'cancel'})]);
  }
}

customElements.define(
    SigninEmailConfirmationAppElement.is, SigninEmailConfirmationAppElement);
