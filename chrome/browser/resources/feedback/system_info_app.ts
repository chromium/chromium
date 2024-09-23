// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/shared/key_value_pair_viewer/key_value_pair_viewer.js';

import type {KeyValuePairEntry} from '/shared/key_value_pair_viewer/key_value_pair_entry.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './css/about_sys.css.js';
import {FeedbackBrowserProxyImpl} from './js/feedback_browser_proxy.js';
import {getHtml} from './system_info_app.html.js';

export interface SystemInfoAppElement {
  $: {
    title: HTMLElement,
  };
}

export class SystemInfoAppElement extends CrLitElement {
  static get is() {
    return 'system-info-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      entries_: {type: Array},
      loading_: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  protected entries_: KeyValuePairEntry[] = [];
  protected loading_: boolean = true;

  override async connectedCallback() {
    super.connectedCallback();
    this.entries_ =
        await FeedbackBrowserProxyImpl.getInstance().getSystemInformation();
    this.loading_ = false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'system-info-app': SystemInfoAppElement;
  }
}

customElements.define(SystemInfoAppElement.is, SystemInfoAppElement);
