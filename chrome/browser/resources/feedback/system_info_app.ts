// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/shared/key_value_pair_viewer/key_value_pair_viewer.js';
import './css/about_sys.css.js';

import type {KeyValuePairEntry} from '/shared/key_value_pair_viewer/key_value_pair_entry.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {FeedbackBrowserProxyImpl} from './js/feedback_browser_proxy.js';
import {getTemplate} from './system_info_app.html.js';

export interface SystemInfoAppElement {
  $: {
    title: HTMLElement,
  };
}

export class SystemInfoAppElement extends PolymerElement {
  static get is() {
    return 'system-info-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      entries_: Array,
      loading_: {
        type: Boolean,
        value: true,
        reflectToAttribute: true,
      },
    };
  }

  private entries_: KeyValuePairEntry[];
  private loading_: boolean;

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
