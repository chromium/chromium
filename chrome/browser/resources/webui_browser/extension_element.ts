// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import '//resources/cr_elements/cr_button/cr_button.js';

import {CrLitElement, type PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {MenuSourceType} from '//resources/mojo/ui/base/mojom/menu_source_type.mojom-webui.js';

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
      visible: {type: Boolean, reflect: true},
    };
  }

  accessor iconUrl: string = '';
  accessor visible: boolean = false;

  private bar: ExtensionsBar;
  private extensionId: string;

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  constructor(extensionId: string, bar: ExtensionsBar) {
    super();
    this.extensionId = extensionId;
    this.bar = bar;
  }

  override update(changedProperties: PropertyValues) {
    this.style.setProperty('--extension-icon-url', `url(${this.iconUrl})`);
    super.update(changedProperties);
  }

  protected onClick() {
    this.bar.onClick(this.extensionId);
  }

  protected onContextMenu(event: PointerEvent) {
    event.preventDefault();
    let sourceType: MenuSourceType = MenuSourceType.kNone;
    switch (event.pointerType) {
      case 'mouse':
        sourceType = MenuSourceType.kMouse;
        break;
      case 'pen':
        sourceType = MenuSourceType.kStylus;
        break;
      case 'touch':
        sourceType = MenuSourceType.kTouch;
    }
    this.bar.onContextMenu(sourceType, this.extensionId);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'webui-browser-extension-element': ExtensionElement;
  }
}

customElements.define(ExtensionElement.is, ExtensionElement);
