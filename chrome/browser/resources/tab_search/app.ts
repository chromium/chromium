// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './tab_search_page.js';

import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import type {ProfileData} from './tab_search.mojom-webui.js';
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
  private listenerIds_: number[] = [];
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

    if (loadTimeData.getBoolean('tabSearchPerformanceImprovements')) {
      const callbackRouter = this.apiProxy_.getCallbackRouter();
      this.listenerIds_.push(callbackRouter.tabsChanged.addListener(
          this.updateAvailableHeight_.bind(this)));
      if (document.visibilityState === 'visible') {
        this.updateAvailableHeight_();
      }
    } else {
      this.updateAvailableHeight_();
      document.addEventListener(
          'visibilitychange', this.documentVisibilityChangedListener_);
    }
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    if (loadTimeData.getBoolean('tabSearchPerformanceImprovements')) {
      this.listenerIds_.forEach(
          id => this.apiProxy_.getCallbackRouter().removeListener(id));
      this.listenerIds_ = [];
    } else {
      document.removeEventListener(
          'visibilitychange', this.documentVisibilityChangedListener_);
    }
  }

  private updateAvailableHeight_(profileData?: ProfileData) {
    if (!profileData) {
      this.apiProxy_.getProfileData().then(
          ({profileData}) => this.updateAvailableHeight_(profileData));
      return;
    }

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
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tab-search-app': TabSearchAppElement;
  }
}

customElements.define(TabSearchAppElement.is, TabSearchAppElement);
