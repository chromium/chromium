// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';

import {CrLitElement, /*html, */ type PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './extension_element.css.js';
import {getHtml} from './extension_element.html.js';
import type {ExtensionsBar} from './extensions_bar.js';

export class ExtensionElement extends CrLitElement {
  static get is() {
    return 'webui-browser-extension-element';
  }

  static override get properties() {
    return {
      iconUrl: {type: String},
    };
  }

  accessor iconUrl: string = '';
  private bar: ExtensionsBar;

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  constructor(id: string, bar: ExtensionsBar) {
    super();
    this.id = id;
    this.bar = bar;
  }

  override update(changedProperties: PropertyValues) {
    this.style.setProperty('--extension-icon-url', `url(${this.iconUrl})`);
    super.update(changedProperties);
  }

  protected onClick() {
    this.bar.onClick(this.id);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'webui-browser-extension-element': ExtensionElement;
  }
}

customElements.define(ExtensionElement.is, ExtensionElement);
