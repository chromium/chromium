// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons.html.js';
import '//resources/cr_elements/cr_toast/cr_toast.js';

import type {CrToastElement} from '//resources/cr_elements/cr_toast/cr_toast.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {UnexportableKeysInternalsBrowserProxyImpl} from './browser_proxy.js';
import type {UnexportableKeysInternalsBrowserProxy} from './browser_proxy.js';
import type {UnexportableKeyInfo} from './unexportable_keys_internals.mojom-webui.js';

export interface UnexportableKeysInternalsAppElement {
  $: {
    deleteErrorToast: CrToastElement,
  };
}

export class UnexportableKeysInternalsAppElement extends CrLitElement {
  static get is() {
    return 'unexportable-keys-internals-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      unexportableKeysInfo_: {type: Array},
    };
  }

  protected accessor unexportableKeysInfo_: UnexportableKeyInfo[] = [];

  private browserProxy_: UnexportableKeysInternalsBrowserProxy =
      UnexportableKeysInternalsBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();
    this.updateKeysList_();
  }

  protected async onDeleteKeyClick_(e: Event) {
    const currentTarget = e.currentTarget as HTMLElement;
    const keyId =
        this.unexportableKeysInfo_[Number(currentTarget.dataset['index'])]!
            .keyId;
    const {success} = await this.browserProxy_.handler.deleteKey(keyId);
    if (!success) {
      this.$.deleteErrorToast.show();
    } else if (this.$.deleteErrorToast.open) {
      // Hide the toast if it was shown before but this time the key has been
      // deleted successfully.
      this.$.deleteErrorToast.hide();
    }
    this.updateKeysList_();
  }

  private async updateKeysList_() {
    const {keys} = await this.browserProxy_.handler.getUnexportableKeysInfo();
    this.unexportableKeysInfo_ = keys;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'unexportable-keys-internals-app': UnexportableKeysInternalsAppElement;
  }
}

customElements.define(
    UnexportableKeysInternalsAppElement.is,
    UnexportableKeysInternalsAppElement);
