// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_input/cr_input.js';

import {getHtml} from './install_dev_proxy_tab.html.js';
import {IwaDevInstallTabElement} from './install_tab.js';

export class IwaDevInstallDevProxyTabElement extends IwaDevInstallTabElement {
  static get is() {
    return 'iwa-dev-install-dev-proxy-tab';
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      disabled: {type: Boolean},
      url_: {type: String, state: true},
      urlError_: {type: String, state: true},
    };
  }

  accessor disabled: boolean = false;
  protected accessor url_: string = '';
  protected accessor urlError_: string = '';

  override reset() {
    this.url_ = '';
    this.urlError_ = '';
  }

  override isValid(): boolean {
    return !!this.url_;
  }

  override submit() {
    this.urlError_ = '';
    if (!this.url_) {
      return;
    }
    if (!this.isValidUrl_(this.url_)) {
      this.urlError_ = 'Please enter a valid URL.';
      return;
    }
    this.fire('request-install-from-dev-proxy', {url: this.url_});
  }

  protected onUrlValueChanged_(e: CustomEvent<{value: string}>) {
    this.url_ = e.detail.value;
    this.urlError_ = '';
    this.notifyValidChanged();
  }

  protected onInputKeydown_(e: KeyboardEvent) {
    if (e.key === 'Enter' && this.isValid()) {
      this.submit();
    }
  }

  private isValidUrl_(url: string): boolean {
    try {
      new URL(url);
      return true;
    } catch {
      return false;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'iwa-dev-install-dev-proxy-tab': IwaDevInstallDevProxyTabElement;
  }
}

customElements.define(
    IwaDevInstallDevProxyTabElement.is, IwaDevInstallDevProxyTabElement);
