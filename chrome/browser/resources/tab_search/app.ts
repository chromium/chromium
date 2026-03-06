// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './tab_search_page.js';

import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import type {TabSearchApiProxy} from './tab_search_api_proxy.js';
import {TabSearchApiProxyImpl} from './tab_search_api_proxy.js';

export class TabSearchAppElement extends CrLitElement {
  static get is() {
    return 'tab-search-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      availableHeight_: {type: Number},
    };
  }

  private apiProxy_: TabSearchApiProxy = TabSearchApiProxyImpl.getInstance();
  private documentVisibilityChangedListener_: () => void;
  protected accessor availableHeight_: number = 0;

  constructor() {
    super();
    this.documentVisibilityChangedListener_ = () => {
      if (document.visibilityState === 'visible') {
        this.updateAvailableHeight_();
      }
    };
  }

  override connectedCallback() {
    super.connectedCallback();

    this.updateAvailableHeight_();
    document.addEventListener(
        'visibilitychange', this.documentVisibilityChangedListener_);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    document.removeEventListener(
        'visibilitychange', this.documentVisibilityChangedListener_);
  }

  private updateAvailableHeight_() {
    this.apiProxy_.getProfileData().then(({profileData}) => {
      // In rare cases there is no browser window. I suspect this happens during
      // browser shutdown.
      if (!profileData.windows || profileData.windows.length === 0) {
        return;
      }
      // TODO(crbug.com/40855872): Determine why no active window is reported
      // in some cases on ChromeOS and Linux.
      const activeWindow = profileData.windows.find((t) => t.active);
      assert(profileData.windows[0]);
      this.availableHeight_ = (activeWindow ?? profileData.windows[0]).height;
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tab-search-app': TabSearchAppElement;
  }
}

customElements.define(TabSearchAppElement.is, TabSearchAppElement);
