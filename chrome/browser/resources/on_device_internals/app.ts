// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


import './event_log.js';
import './tools.js';
import '//resources/cr_elements/cr_page_selector/cr_page_selector.js';
import '//resources/cr_elements/cr_tabs/cr_tabs.js';

import type {CrTabsElement} from '//resources/cr_elements/cr_tabs/cr_tabs.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';

export interface OnDeviceInternalsAppElement {
  $: {
    'tabs': CrTabsElement,
  };
}

export class OnDeviceInternalsAppElement extends CrLitElement {
  static get is() {
    return 'on-device-internals-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      selectedTabIndex_: {type: Number},
    };
  }

  protected selectedTabIndex_: number = 0;

  protected onSelectedIndexChange_(e: CustomEvent<{value: number}>) {
    this.selectedTabIndex_ = e.detail.value;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'on-device-internals-app': OnDeviceInternalsAppElement;
  }
}

customElements.define(
    OnDeviceInternalsAppElement.is, OnDeviceInternalsAppElement);
