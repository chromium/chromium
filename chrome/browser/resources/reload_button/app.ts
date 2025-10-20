// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons.html.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {BrowserProxyImpl} from './browser_proxy.js';

export class ReloadButtonAppElement extends CrLitElement {
  constructor() {
    super();
    BrowserProxyImpl.getInstance().callbackRouter.setLoadingState.addListener(
        (isLoading: boolean) => {
          this.isLoading_ = isLoading;
        });
  }

  static get is() {
    return 'reload-button-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      isLoading_: {state: true, type: Boolean},
    };
  }

  protected accessor isLoading_: boolean = false;

  // TODO(crbug.com/444358999): implement the reload logic
  protected onReloadOrStopClick_(_: Event) {
    if (this.isLoading_) {
      BrowserProxyImpl.getInstance().handler.stopReload();
    } else {
      BrowserProxyImpl.getInstance().handler.reload();
    }
    // Update the renderer in advance to avoid the delay.
    this.isLoading_ = !this.isLoading_;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'reload-button-app': ReloadButtonAppElement;
  }
}

customElements.define(ReloadButtonAppElement.is, ReloadButtonAppElement);
