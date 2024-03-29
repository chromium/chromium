// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/shared/key_value_pair_viewer/key_value_pair_viewer.js';
import './strings.m.js';

import type {KeyValuePairEntry} from '/shared/key_value_pair_viewer/key_value_pair_entry.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';
import type {SystemLog} from './browser_proxy.js';
import {BrowserProxyImpl} from './browser_proxy.js';

export interface SystemAppElement {
  $: {
    title: HTMLElement,
  };
}

export class SystemAppElement extends PolymerElement {
  static get is() {
    return 'system-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      entries_: Array,
      loading_: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },
    };
  }

  private entries_: KeyValuePairEntry[];
  // <if expr="chromeos_ash">
  private isLacrosEnabled_: boolean;
  // </if>
  private loading_: boolean;

  override async connectedCallback() {
    super.connectedCallback();

    // <if expr="chromeos_ash">
    this.isLacrosEnabled_ =
        await BrowserProxyImpl.getInstance().isLacrosEnabled();
    // </if>

    this.loading_ = true;
    const logs = await BrowserProxyImpl.getInstance().requestSystemInfo();
    this.entries_ = logs.map((log: SystemLog) => {
      return {
        key: log.statName,
        value: log.statValue,
      };
    });
    this.loading_ = false;

    // Dispatch event used by tests.
    this.dispatchEvent(new CustomEvent('ready-for-testing'));
  }

  // <if expr="chromeos_ash">
  private onOsLinkContainerClick_(event: MouseEvent) {
    this.handleOsLinkContainerClick_(event);
  }
  private onOsLinkContainerAuxClick_(event: MouseEvent) {
    // Make middle-clicks have the same effects as Ctrl+clicks
    if (event.button === 1) {
      this.handleOsLinkContainerClick_(event);
    }
  }
  private handleOsLinkContainerClick_(event: MouseEvent) {
    if (event.target instanceof Element && event.target.id === 'osLinkHref') {
      event.preventDefault();
      BrowserProxyImpl.getInstance().openLacrosSystemPage();
    }
  }
  // </if>
}

declare global {
  interface HTMLElementTagNameMap {
    'system-app': SystemAppElement;
  }
}

customElements.define(SystemAppElement.is, SystemAppElement);
