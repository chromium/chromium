// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getHtml} from './install_local_bundle_tab.html.js';
import {IwaDevInstallTabElement} from './install_tab.js';

export class IwaDevInstallLocalBundleTabElement extends
    IwaDevInstallTabElement {
  static get is() {
    return 'iwa-dev-install-local-bundle-tab';
  }

  override connectedCallback() {
    super.connectedCallback();
    this.notifyValidChanged();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      disabled: {type: Boolean},
    };
  }

  accessor disabled: boolean = false;

  override reset() {}

  override isValid(): boolean {
    return true;
  }

  override submit() {
    this.fire('request-install-from-local-bundle');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'iwa-dev-install-local-bundle-tab': IwaDevInstallLocalBundleTabElement;
  }
}

customElements.define(
    IwaDevInstallLocalBundleTabElement.is, IwaDevInstallLocalBundleTabElement);
