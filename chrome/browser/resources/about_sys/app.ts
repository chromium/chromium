// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/shared/key_value_pair_viewer/key_value_pair_viewer.js';
import './strings.m.js';

import type {KeyValuePairEntry} from '/shared/key_value_pair_viewer/key_value_pair_entry.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import type {SystemLog} from './browser_proxy.js';
import {BrowserProxyImpl} from './browser_proxy.js';

export interface AppElement {
  $: {
    title: HTMLElement,
  };
}

export class AppElement extends CrLitElement {
  static get is() {
    return 'system-app';
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

      // <if expr="chromeos_ash">
      isLacrosEnabled_: {type: Boolean},
      // </if>

      loading_: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  protected entries_: KeyValuePairEntry[] = [];
  // <if expr="chromeos_ash">
  protected isLacrosEnabled_: boolean = false;
  // </if>
  protected loading_: boolean = false;

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
  protected onOsLinkContainerClick_(event: MouseEvent) {
    this.handleOsLinkContainerClick_(event);
  }

  protected onOsLinkContainerAuxClick_(event: MouseEvent) {
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
    'system-app': AppElement;
  }
}

customElements.define(AppElement.is, AppElement);
