// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {ExtensionBrowserProxyImpl} from 'chrome://contextual-tasks/contextual_tasks_browser_proxy.js';
import type {PageHandlerInterface} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
import {ComposeboxProxyImpl} from 'chrome://resources/cr_components/composebox/composebox_proxy.js';
import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {CSSResultGroup} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './lens_button_app.css.js';
import {getHtml} from './lens_button_app.html.js';

export interface LensButtonAppElement {
  $: {
    lensButton: CrIconButtonElement,
  };
}

export class LensButtonAppElement extends CrLitElement {
  static get is() {
    return 'lens-button-app';
  }

  static override get styles(): CSSResultGroup {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      active: {
        type: Boolean,
        reflect: true,
      },
      disabled: {
        type: Boolean,
        reflect: true,
      },
      label_: {type: String},
    };
  }

  accessor active: boolean = false;
  accessor disabled: boolean = false;
  protected accessor label_: string = loadTimeData.isInitialized() &&
          loadTimeData.valueExists('lensSearchButtonLabel') ?
      loadTimeData.getString('lensSearchButtonLabel') :
      '';

  private pageHandler_: PageHandlerInterface|null = null;
  private overlayStateListenerId_: number|null = null;

  override connectedCallback() {
    super.connectedCallback();

    const browserProxy = ExtensionBrowserProxyImpl.getInstance();
    if (this.overlayStateListenerId_ === null) {
      this.overlayStateListenerId_ =
          browserProxy.callbackRouter.onLensOverlayStateChanged.addListener(
              (isShowing: boolean) => {
                this.active = isShowing;
              });
    }
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    if (this.overlayStateListenerId_ !== null) {
      const browserProxy = ExtensionBrowserProxyImpl.getInstance();
      browserProxy.callbackRouter.removeListener(this.overlayStateListenerId_);
      this.overlayStateListenerId_ = null;
    }
  }

  setPageHandlerForTesting(handler: PageHandlerInterface|null) {
    this.pageHandler_ = handler;
  }

  private getPageHandler_(): PageHandlerInterface {
    return this.pageHandler_ || ComposeboxProxyImpl.getInstance().handler;
  }

  protected onClick_() {
    if (this.disabled) {
      return;
    }
    this.getPageHandler_().handleLensButtonClick();
  }

  protected onMousedown_(e: MouseEvent) {
    e.preventDefault();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'lens-button-app': LensButtonAppElement;
  }
}

customElements.define(LensButtonAppElement.is, LensButtonAppElement);
