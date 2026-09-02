// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/localized_link/localized_link.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import 'chrome://resources/cr_elements/icons.html.js';
import '/strings.m.js';

import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
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
      isMetricsEnabled_: {type: Boolean},
      setDefaultBrowser_: {type: Boolean},
    };
  }

  protected accessor setDefaultBrowser_: boolean | null =
      loadTimeData.getBoolean('showDefaultBrowserToggle') ? true : null;
  private accessor anyButtonClicked_: boolean = false;
  private accessor isMetricsEnabled_: boolean | null =
      loadTimeData.getBoolean('showMetricsOptIn') ? true : null;

  protected showDefaultBrowserToggle_: boolean =
      loadTimeData.getBoolean('showDefaultBrowserToggle');
  protected showMetricsOptIn_: boolean =
      loadTimeData.getBoolean('showMetricsOptIn');
  private browserProxy_: WelcomeBrowserProxy =
      welcomeMojoProxyFactory.getInstance();

  constructor() {
    super();
    ColorChangeUpdater.forDocument().start();
  }

  private getDialog_(): HTMLDialogElement | null {
    return this.shadowRoot.querySelector<HTMLDialogElement>('#dialog');
  }

  protected getDialogActionButtonLabel_(): string {
    return this.isMetricsEnabled_ ?
        this.i18n('welcomeMetricsPopupTurnOffButtonLabel') :
        this.i18n('welcomeMetricsPopupTurnOnButtonLabel');
  }

  protected getMetricsLabel_(): TrustedHTML {
    return this.isMetricsEnabled_ ?
        this.i18nAdvanced('welcomeMetricsLabel') :
        this.i18nAdvanced('welcomeMetricsOffLabel');
  }

  protected shouldDisableButtons_(): boolean {
    return this.anyButtonClicked_;
  }

  protected onDefaultBrowserCheckedChanged_(e: CustomEvent<{value: boolean}>) {
    this.setDefaultBrowser_ = e.detail.value;
  }

  protected onAcceptButtonClick_() {
    this.anyButtonClicked_ = true;
    this.browserProxy_.handler.continue(
        this.isMetricsEnabled_, this.setDefaultBrowser_);
  }

  protected onManageLinkClicked_(e: CustomEvent<{event: Event}>) {
    e.detail.event.preventDefault();
    const dialog = this.getDialog_();
    assert(dialog);
    dialog.showModal();
  }

  protected onDialogCloseButtonClick_() {
    const dialog = this.getDialog_();
    assert(dialog);
    dialog.close();
  }

  protected onDialogActionButtonClick_() {
    this.isMetricsEnabled_ = !this.isMetricsEnabled_;
    const dialog = this.getDialog_();
    assert(dialog);
    dialog.close();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'welcome-app': WelcomeAppElement;
  }
}

customElements.define(WelcomeAppElement.is, WelcomeAppElement);
