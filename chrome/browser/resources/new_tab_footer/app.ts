// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {assert} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {NewTabFooterDocumentProxy} from './browser_proxy.js';
import type {ExtensionAttribution, ManagementNotice, NewTabFooterDocumentCallbackRouter} from './new_tab_footer.mojom-webui.js';

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
      extensionAttribution_: {type: Object},
      managementNotice_: {type: Object},
    };
  }


  protected accessor extensionAttribution_: ExtensionAttribution|null = null;
  protected accessor managementNotice_: ManagementNotice|null = null;
  private callbackRouter_: NewTabFooterDocumentCallbackRouter;
  private setManagementNoticeListener_: number|null = null;

  constructor() {
    super();
    this.callbackRouter_ =
        NewTabFooterDocumentProxy.getInstance().callbackRouter;
    this.getNtpExtensionAttribution_();
  }

  override firstUpdated() {
    ColorChangeUpdater.forDocument().start();
  }

  override connectedCallback() {
    super.connectedCallback();
    this.setManagementNoticeListener_ =
        this.callbackRouter_.setManagementNotice.addListener(
            (notice: ManagementNotice) => {
              if (notice) {
                this.managementNotice_ = notice;
              }
            });
    NewTabFooterDocumentProxy.getInstance().handler.updateManagementNotice();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    assert(this.setManagementNoticeListener_);
    this.callbackRouter_.removeListener(this.setManagementNoticeListener_);
  }

  private async getNtpExtensionAttribution_() {
    this.extensionAttribution_ = (await NewTabFooterDocumentProxy.getInstance()
                                      .handler.getNtpExtensionAttribution())
                                     .attribution;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'new-tab-footer-app': NewTabFooterAppElement;
  }
}

customElements.define(NewTabFooterAppElement.is, NewTabFooterAppElement);
