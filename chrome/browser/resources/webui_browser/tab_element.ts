// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './tab_element.css.js';
import {getHtml} from './tab_element.html.js';
import type {TabStrip} from './tab_strip.js';
import type {Tab as TabData} from './tab_strip_api_data_model.mojom-webui.js';
import type {NodeId} from './tab_strip_api_types.mojom-webui.js';

export class TabElement extends CrLitElement {
  static get is() {
    // cannot use "tab" because custom element name must contain a hyphen "-".
    return 'webui-browser-tab';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override properties = {
    active: {type: Boolean},
  };

  tabId: NodeId;
  active: boolean = false;
  faviconUrl: string = 'chrome://favicon2/';
  tabTitle: string = '';

  constructor(tab: TabData) {
    super();
    this.tabId = tab.id;
    this.updateData(tab);
  }

  override update(changedProperties: PropertyValues) {
    this.style.setProperty('--favicon-url', `url(${this.faviconUrl})`);
    super.update(changedProperties);
  }

  updateData(tab: TabData) {
    /* TODO(webium): tab.active is not present anymore
    this.active = tab.active;
    */
    /* TODO(webium): tab.favicon is an Image now, not a URL
    if (this.active && tab.activeFaviconUrl) {
      this.faviconUrl = tab.activeFaviconUrl.url;
    } else if (tab.faviconUrl) {
      this.faviconUrl = tab.faviconUrl.url;
    }
    */
    this.tabTitle = tab.title;
    this.requestUpdate();
  }

  // Calculating the dom matrix value could be expensive.
  // This potentially could just be stored in the Tab and then updated manually
  // by the tabstrip during animation.
  getTransformX() {
    const matrix = new DOMMatrixReadOnly(this.style.transform);
    return matrix.m41;
  }

  protected handleClick(e: MouseEvent) {
    e = e || window.event;
    e.preventDefault();
    this.dispatchEvent(new CustomEvent(
        'tab-click',
        {bubbles: true, composed: true, detail: {tabId: this.tabId}}));
  }

  protected handleClose() {
    this.dispatchEvent(new CustomEvent(
        'tab-close',
        {bubbles: true, composed: true, detail: {tabId: this.tabId}}));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'webui-browser-tabstrip': TabStrip;
    'webui-browser-tab': TabElement;
  }
}

customElements.define(TabElement.is, TabElement);
