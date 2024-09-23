// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';
import './strings.m.js';

import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './signin_error_app.css.js';
import {getHtml} from './signin_error_app.html.js';


const SigninErrorAppElementBase = WebUiListenerMixinLit(CrLitElement);

export class SigninErrorAppElement extends SigninErrorAppElementBase {
  static get is() {
    return 'signin-error-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      fromProfilePicker_: {type: Boolean},
      switchButtonUnavailable_: {type: Boolean},
      hideNormalError_: {type: Boolean},

      /**
       * An array of booleans indicating whether profile blocking messages
       * should be hidden. Position 0 corresponds to the
       * #profile-blocking-error-message container, and subsequent positions
       * correspond to each of the 3 related messages respectively.
       */
      hideProfileBlockingErrors_: {type: Array},
    };
  }

  protected fromProfilePicker_: boolean =
      loadTimeData.getBoolean('fromProfilePicker');
  protected switchButtonUnavailable_: boolean = false;
  protected hideNormalError_: boolean =
      loadTimeData.getString('signinErrorMessage').length === 0;
  protected hideProfileBlockingErrors_: boolean[];

  constructor() {
    super();

    this.hideProfileBlockingErrors_ = (function() {
      const hide = [
        'profileBlockedMessage',
        'profileBlockedAddPersonSuggestion',
        'profileBlockedRemoveProfileSuggestion',
      ].map(id => loadTimeData.getString(id).length === 0);

      // Hide the container itself if all of each children are also hidden.
      hide.unshift(hide.every(hideEntry => hideEntry));

      return hide;
    })();
  }

  override connectedCallback() {
    super.connectedCallback();

    this.addWebUiListener('switch-button-unavailable', async () => {
      this.switchButtonUnavailable_ = true;
      await this.updateComplete;
      // Move focus to the only displayed button in this case.
      const button =
          this.shadowRoot!.querySelector<HTMLElement>('#confirmButton');
      assert(button);
      button.focus();
    });
  }

  protected onConfirm_() {
    chrome.send('confirm');
  }

  protected onSwitchToExistingProfile_() {
    chrome.send('switchToExistingProfile');
  }

  protected onLearnMore_() {
    chrome.send('learnMore');
  }
}

customElements.define(SigninErrorAppElement.is, SigninErrorAppElement);
