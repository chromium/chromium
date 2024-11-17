// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getHtml} from './app.html.js';
import {BrowserProxy} from './browser_proxy.js';
import type {LanguagePackInfo} from './on_device_translation_internals.mojom-webui.js';
import {LanguagePackStatus} from './on_device_translation_internals.mojom-webui.js';

export class OnDeviceTranslationInternalsAppElement extends CrLitElement {
  static get is() {
    return 'on-device-translation-internals-app';
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      languagePackStatus_: {type: Array},
    };
  }

  private proxy_: BrowserProxy = BrowserProxy.getInstance();
  private onLanguagePackStatusListenerId_: number|null = null;
  protected languagePackStatus_: LanguagePackInfo[] = [];

  override connectedCallback() {
    super.connectedCallback();
    this.onLanguagePackStatusListenerId_ =
        this.proxy_.callbackRouter.onLanguagePackStatus.addListener(
            this.onLanguagePackStatus_.bind(this));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    assert(this.onLanguagePackStatusListenerId_);
    this.proxy_.callbackRouter.removeListener(
        this.onLanguagePackStatusListenerId_);
  }

  private onLanguagePackStatus_(status: LanguagePackInfo[]) {
    this.languagePackStatus_ = status;
  }

  protected getStatusString_(status: LanguagePackStatus): string {
    switch (status) {
      case LanguagePackStatus.kNotInstalled:
        return 'Not installed';
      case LanguagePackStatus.kInstalling:
        return 'Installing';
      case LanguagePackStatus.kInstalled:
        return 'Installed';
      default:
        assertNotReached('Invalid status type.');
    }
  }

  protected getButtonString_(status: LanguagePackStatus): string {
    return status === LanguagePackStatus.kNotInstalled ? 'Install' :
                                                         'Uninstall';
  }

  protected onButtonClick_(e: Event) {
    const target = e.currentTarget as HTMLElement;
    const index = Number(target.dataset['index']);
    if (this.languagePackStatus_[index]!.status ===
        LanguagePackStatus.kNotInstalled) {
      this.proxy_.handler.installLanguagePackage(index);
    } else {
      this.proxy_.handler.uninstallLanguagePackage(index);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'on-device-translation-internals-app':
        OnDeviceTranslationInternalsAppElement;
  }
}

customElements.define(
    OnDeviceTranslationInternalsAppElement.is,
    OnDeviceTranslationInternalsAppElement);
