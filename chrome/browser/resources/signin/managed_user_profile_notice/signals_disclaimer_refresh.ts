// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/icons.html.js';
import '/icons.html.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {ManagedUserProfileNoticeBrowserProxyImpl} from './managed_user_profile_notice_browser_proxy.js';
import {getCss} from './signals_disclaimer_refresh.css.js';
import {getHtml} from './signals_disclaimer_refresh.html.js';

export interface SignalsDisclaimerRefreshElement {
  $: {
    avatar: HTMLImageElement,
    title: HTMLElement,
    subtitle: HTMLElement,
    learnMoreLink: HTMLElement,
  };
}

export class SignalsDisclaimerRefreshElement extends CrLitElement {
  static get is() {
    return 'signals-disclaimer-refresh';
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
    };
  }

  accessor pictureUrl: string = '';

  override firstUpdated() {
    this.$.title.focus();
  }

  protected onLearnMoreClick_() {
    ManagedUserProfileNoticeBrowserProxyImpl.getInstance().learnMoreClicked();
  }

  protected onLearnMoreKeydown_(e: KeyboardEvent) {
    if (e.key === 'Enter' || e.key === ' ') {
      e.preventDefault();
      this.onLearnMoreClick_();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'signals-disclaimer-refresh': SignalsDisclaimerRefreshElement;
  }
}

customElements.define(
    SignalsDisclaimerRefreshElement.is, SignalsDisclaimerRefreshElement);
