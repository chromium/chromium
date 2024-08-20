// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.js';
import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.js';
import './strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './signin_email_confirmation_app.css.js';
import {getHtml} from './signin_email_confirmation_app.html.js';

export interface SigninEmailConfirmationAppElement {
  $: {
    dialogTitle: HTMLElement,
    createNewUserRadioButtonSubtitle: HTMLElement,
    startSyncRadioButtonSubtitle: HTMLElement,
  };
}

export class SigninEmailConfirmationAppElement extends CrLitElement {
  static get is() {
    return 'signin-email-confirmation-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  override firstUpdated() {
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

  protected onConfirm_() {
    const action = this.shadowRoot!.querySelector('cr-radio-group')!.selected;
    chrome.send('dialogClose', [JSON.stringify({'action': action})]);
  }

  protected onCancel_() {
    chrome.send('dialogClose', [JSON.stringify({'action': 'cancel'})]);
  }
}

customElements.define(
    SigninEmailConfirmationAppElement.is, SigninEmailConfirmationAppElement);
