// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/icons.html.js';
import '/strings.m.js';

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
      switchButtonUnavailable_: {type: Boolean},
      hideNormalError_: {type: Boolean},
    };
  }

  protected accessor switchButtonUnavailable_: boolean = false;
  protected accessor hideNormalError_: boolean =
      loadTimeData.getString('signinErrorMessage').length === 0;

  constructor() {
    super();
  }

  override connectedCallback() {
    super.connectedCallback();

    this.addWebUiListener('switch-button-unavailable', async () => {
      this.switchButtonUnavailable_ = true;
      await this.updateComplete;
      // Move focus to the only displayed button in this case.
      const button =
          this.shadowRoot.querySelector<HTMLElement>('#confirmButton');
      assert(button);
      button.focus();
    });
  }

  protected onConfirmClick_() {
    chrome.send('confirm');
  }

  protected onSwitchToExistingProfileClick_() {
    chrome.send('switchToExistingProfile');
  }

  protected onLearnMoreClick_() {
    chrome.send('learnMore');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'signin-error-app': SigninErrorAppElement;
  }
}

customElements.define(SigninErrorAppElement.is, SigninErrorAppElement);
