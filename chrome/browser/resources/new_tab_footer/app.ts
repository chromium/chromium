// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {NewTabFooterDocumentProxy} from './browser_proxy.js';
import type {ManagementNotice, NewTabFooterDocumentCallbackRouter, NewTabFooterHandlerInterface} from './new_tab_footer.mojom-webui.js';


export class NewTabFooterAppElement extends CrLitElement {
  static get is() {
    return 'new-tab-footer-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      extensionName_: {type: String},
      managementNotice_: {type: Object},
    };
  }

  protected accessor extensionName_: string|null = null;
  protected accessor managementNotice_: ManagementNotice|null = null;

  private callbackRouter_: NewTabFooterDocumentCallbackRouter;
  private handler_: NewTabFooterHandlerInterface;
  private setNtpExtensionNameListenerId_: number|null = null;
  private setManagementNoticeListener_: number|null = null;

  constructor() {
    super();
    this.callbackRouter_ =
        NewTabFooterDocumentProxy.getInstance().callbackRouter;
    this.handler_ = NewTabFooterDocumentProxy.getInstance().handler;
  }

  override firstUpdated() {
    ColorChangeUpdater.forDocument().start();
  }

  override connectedCallback() {
    super.connectedCallback();
    this.setNtpExtensionNameListenerId_ =
        this.callbackRouter_.setNtpExtensionName.addListener((name: string) => {
          this.extensionName_ = name;
        });
    this.handler_.updateNtpExtensionName();
    this.setManagementNoticeListener_ =
        this.callbackRouter_.setManagementNotice.addListener(
            (notice: ManagementNotice) => {
                this.managementNotice_ = notice;
            });
    this.handler_.updateManagementNotice();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    assert(this.setNtpExtensionNameListenerId_);
    this.callbackRouter_.removeListener(this.setNtpExtensionNameListenerId_);
    assert(this.setManagementNoticeListener_);
    this.callbackRouter_.removeListener(this.setManagementNoticeListener_);
  }

  protected onExtensionNameClick_(e: Event) {
    e.preventDefault();
    this.handler_.openExtensionOptionsPageWithFallback();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'new-tab-footer-app': NewTabFooterAppElement;
  }
}

customElements.define(NewTabFooterAppElement.is, NewTabFooterAppElement);
