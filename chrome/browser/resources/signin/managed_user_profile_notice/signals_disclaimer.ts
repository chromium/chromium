// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/icons.html.js';
import '/icons.html.js';

import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {ManagedUserProfileNoticeBrowserProxyImpl} from './managed_user_profile_notice_browser_proxy.js';
import {getCss} from './signals_disclaimer.css.js';
import {getHtml} from './signals_disclaimer.html.js';

const SignalsDisclaimerElementBase = I18nMixinLit(CrLitElement);

export class SignalsDisclaimerElement extends SignalsDisclaimerElementBase {
  static get is() {
    return 'signals-disclaimer';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      pictureUrl: {type: String},
      isModalDialog: {type: Boolean},
    };
  }

  accessor pictureUrl: string = '';
  accessor isModalDialog: boolean = loadTimeData.getBoolean('isModalDialog');

  override firstUpdated() {
    const titleElement = this.shadowRoot.querySelector<HTMLElement>('.title');
    assert(titleElement);
    titleElement.focus();
  }

  protected onLearnMoreClick() {
    ManagedUserProfileNoticeBrowserProxyImpl.getInstance().learnMoreClicked();
  }

  protected onLearnMoreKeydown(e: KeyboardEvent) {
    if (e.key === 'Enter' || e.key === ' ') {
      e.preventDefault();
      this.onLearnMoreClick();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'signals-disclaimer': SignalsDisclaimerElement;
  }
}

customElements.define(SignalsDisclaimerElement.is, SignalsDisclaimerElement);
