// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/icons.html.js';

import type {ExtensionPageHandlerInterface} from 'chrome://contextual-tasks/contextual_tasks.mojom-webui.js';
import {ExtensionBrowserProxyImpl} from 'chrome://contextual-tasks/contextual_tasks_browser_proxy.js';
import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {CSSResultGroup} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './lens_chip_app.css.js';
import {getHtml} from './lens_chip_app.html.js';

export interface LensChipAppElement {
  $: {
    chipRoot?: HTMLElement,
    cropImage?: HTMLImageElement,
    closeButton?: CrIconButtonElement,
  };
}

export class LensChipAppElement extends CrLitElement {
  static get is() {
    return 'lens-chip-app';
  }

  static override get styles(): CSSResultGroup {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      dataUri: {type: String},
      darkMode: {
        type: Boolean,
        reflect: true,
        attribute: 'dark-mode',
      },
      label_: {type: String},
      dismissLabel_: {type: String},
    };
  }

  protected accessor dataUri: string = '';
  protected accessor darkMode: boolean = false;

  protected accessor label_: string =
      loadTimeData.getString('lensRegionChipLabel');

  protected accessor dismissLabel_: string =
      loadTimeData.getString('lensRegionChipDismissA11yLabel');

  private pageHandler_: ExtensionPageHandlerInterface|null = null;

  override connectedCallback() {
    super.connectedCallback();

    const params = new URLSearchParams(window.location.search);
    if (params.has('cs')) {
      this.darkMode = params.get('cs') === '1';
    }

    this.fetchPreview_();
  }

  setPageHandlerForTesting(handler: ExtensionPageHandlerInterface|null) {
    this.pageHandler_ = handler;
  }

  setDataUriForTesting(dataUri: string) {
    this.dataUri = dataUri;
  }

  getDataUriForTesting(): string {
    return this.dataUri;
  }

  setDarkModeForTesting(darkMode: boolean) {
    this.darkMode = darkMode;
  }

  getDarkModeForTesting(): boolean {
    return this.darkMode;
  }

  private getPageHandler_(): ExtensionPageHandlerInterface {
    return this.pageHandler_ || ExtensionBrowserProxyImpl.getInstance().handler;
  }

  private async fetchPreview_() {
    try {
      const response = await this.getPageHandler_().getLensCropPreview();
      this.dataUri = response?.dataUri ?? '';
    } catch (e) {
      this.dataUri = '';
    }
  }

  protected async onCloseClick_() {
    this.fire('close-chip');
    if (this.dataUri) {
      this.dataUri = '';
      try {
        await this.getPageHandler_().removeLensCrop();
      } catch (e) {
        // Ignore
      }
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'lens-chip-app': LensChipAppElement;
  }
}

customElements.define(LensChipAppElement.is, LensChipAppElement);
