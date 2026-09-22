// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {BrowserProxy} from './browser_proxy.js';

export class DefaultBrowserModalAppElement extends CrLitElement {
  static get is() {
    return 'default-browser-modal-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      useSettingsIllustration: {
        type: Boolean,
        reflect: true,
      },
      hasAccepted_: {type: Boolean},
    };
  }

  private boundOnFocus_: () => void = () => this.onWindowFocus_();
  private hasAcceptedListenerId_: number|null = null;

  override connectedCallback() {
    super.connectedCallback();
    if (!this.isStickyModal_) {
      return;
    }

    window.addEventListener('focus', this.boundOnFocus_);
    this.hasAcceptedListenerId_ =
        BrowserProxy.getInstance()
            .callbackRouter.onHasAcceptedChanged.addListener(
                (hasAccepted: boolean) => {
                  this.hasAccepted_ = hasAccepted;
                });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    if (!this.isStickyModal_) {
      return;
    }

    window.removeEventListener('focus', this.boundOnFocus_);

    if (this.hasAcceptedListenerId_ !== null) {
      BrowserProxy.getInstance().callbackRouter.removeListener(
          this.hasAcceptedListenerId_);
      this.hasAcceptedListenerId_ = null;
    }
  }

  override firstUpdated() {
    ColorChangeUpdater.forDocument().start();

    if (!this.isModal_) {
      return;
    }
    BrowserProxy.getInstance().handler.showUI();
  }

  accessor useSettingsIllustration: boolean =
      loadTimeData.getBoolean('useSettingsIllustration');

  protected accessor hasAccepted_: boolean = false;
  private isStickyModal_: boolean = loadTimeData.getBoolean('isStickyModal');

  private isModal_: boolean = loadTimeData.getBoolean('isModal');

  protected showRetryAndClose_(): boolean {
    return this.isStickyModal_ && this.hasAccepted_;
  }

  protected onCancelClick_() {
    BrowserProxy.getInstance().handler.cancel();
  }

  protected onConfirmClick_() {
    BrowserProxy.getInstance().handler.confirm();
  }

  protected onTryAgainClick_() {
    BrowserProxy.getInstance().handler.tryAgain();
  }

  protected onCloseClick_() {
    BrowserProxy.getInstance().handler.cancel();
  }

  private async onWindowFocus_() {
    if (document.visibilityState === 'hidden') {
      return;
    }
    if (!this.hasAccepted_) {
      return;
    }

    await BrowserProxy.getInstance().handler.checkDefaultStatusAndMaybeClose();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'default-browser-modal-app': DefaultBrowserModalAppElement;
  }
}

customElements.define(
    DefaultBrowserModalAppElement.is, DefaultBrowserModalAppElement);
