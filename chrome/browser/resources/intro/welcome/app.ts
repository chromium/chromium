// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import '/strings.m.js';

import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {browserProxyFactory as welcomeMojoProxyFactory} from '../welcome.mojom-webui.js';
import type {BrowserProxy as WelcomeBrowserProxy} from '../welcome.mojom-webui.js';

export interface WelcomeAppElement {
  $: {
    acceptButton: CrButtonElement,
  };
}

const WelcomeAppElementBase = I18nMixinLit(CrLitElement);

export class WelcomeAppElement extends WelcomeAppElementBase {
  static get is(): string {
    return 'welcome-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      anyButtonClicked_: {type: Boolean},
    };
  }

  private accessor anyButtonClicked_: boolean = false;
  private browserProxy_: WelcomeBrowserProxy =
      welcomeMojoProxyFactory.getInstance();

  constructor() {
    super();
    ColorChangeUpdater.forDocument().start();
  }


  protected shouldDisableButtons_(): boolean {
    return this.anyButtonClicked_;
  }

  protected onAcceptButtonClick_() {
    this.anyButtonClicked_ = true;
    // TODO(crbug.com/542895787): Pass in UMA and default browser state.
    this.browserProxy_.handler.continue(null, null);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'welcome-app': WelcomeAppElement;
  }
}

customElements.define(WelcomeAppElement.is, WelcomeAppElement);
