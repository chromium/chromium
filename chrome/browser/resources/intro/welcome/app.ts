// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import '/strings.m.js';

import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
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
      setDefaultBrowser_: {type: Boolean},
      showDefaultBrowserToggle_: {type: Boolean},
    };
  }

  protected accessor showDefaultBrowserToggle_: boolean =
      loadTimeData.getBoolean('showDefaultBrowserToggle');
  protected accessor setDefaultBrowser_: boolean | null =
      this.showDefaultBrowserToggle_ ? true : null;
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

  protected onDefaultBrowserCheckedChanged_(e: CustomEvent<{value: boolean}>) {
    this.setDefaultBrowser_ = e.detail.value;
  }

  protected onAcceptButtonClick_() {
    this.anyButtonClicked_ = true;
    // TODO(crbug.com/542895787): Pass in UMA opt-in state.
    this.browserProxy_.handler.continue(null, this.setDefaultBrowser_);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'welcome-app': WelcomeAppElement;
  }
}

customElements.define(WelcomeAppElement.is, WelcomeAppElement);
