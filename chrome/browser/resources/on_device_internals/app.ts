// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


import '//resources/cr_elements/cr_tab_box/cr_tab_box.js';
import './tools.js';

import type {CrTabBoxElement} from '//resources/cr_elements/cr_tab_box/cr_tab_box.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';

export interface OnDeviceInternalsAppElement {
  $: {
    'tabbox': CrTabBoxElement,
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

  private tabPanelIds_: string[] = [];

  override firstUpdated() {
    const tabpanels = this.$.tabbox.querySelectorAll('div[slot=\'panel\']');
    this.tabPanelIds_ = Array.from(tabpanels, tab => tab.id);
  }

  override connectedCallback() {
    super.connectedCallback();
    window.addEventListener('hashchange', this.activateTabByHash_.bind(this));
    this.activateTabByHash_();
  }


  override disconnectedCallback() {
    super.disconnectedCallback();
    window.removeEventListener(
        'hashchange', this.activateTabByHash_.bind(this));
  }

  protected onSelectedIndexChange_(e: CustomEvent<number>) {
    if (this.tabPanelIds_.length === 0) {
      // Skip when called before `firstUpdated` has populated the ids.
      return;
    }
    window.location.hash = this.tabPanelIds_[e.detail];
  }

  private activateTabByHash_() {
    // Remove the first character '#'.
    const hash = window.location.hash.substring(1);
    const index = this.tabPanelIds_.indexOf(hash);
    if (index === -1) {
      return;
    }
    this.$.tabbox.setAttribute('selected-index', `${index}`);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'on-device-internals-app': OnDeviceInternalsAppElement;
  }
}

customElements.define(
    OnDeviceInternalsAppElement.is, OnDeviceInternalsAppElement);
