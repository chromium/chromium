// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '/shared/icon_from_table.js';

import {TrackedElementManager} from '//resources/js/tracked_element/tracked_element_manager.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import {MenuSourceType} from '//resources/mojo/ui/base/mojom/menu_source_type.mojom-webui.js';
import type {IconHandle} from '/shared/icon_handle.mojom-webui.js';

import {getCss} from './extension.css.js';
import {getHtml} from './extension.html.js';
import type {ExtensionsBarElement} from './extensions_bar.js';

export class ExtensionElement extends CrLitElement {
  static get is() {
    return 'webui-browser-extension';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      iconHandle: {type: Object},
      visible: {type: Boolean, reflect: true},
    };
  }

  accessor iconHandle: IconHandle = {handleId: 0n};
  accessor visible: boolean = false;

  private bar: ExtensionsBarElement;
  private extensionId: string;

  constructor(extensionId: string, bar: ExtensionsBarElement) {
    super();
    this.extensionId = extensionId;
    this.bar = bar;
  }

  override connectedCallback() {
    super.connectedCallback();
    TrackedElementManager.getInstance().startTracking(
        this, 'kToolbarActionViewElementId',
        {secondaryId: 'ext:' + this.extensionId});
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    TrackedElementManager.getInstance().stopTracking(this);
  }

  protected onClick() {
    this.bar.onClick(this.extensionId);
  }

  protected onContextmenu_(event: PointerEvent) {
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
        break;
      default:
        break;
    }
    this.bar.onContextMenu(sourceType, this.extensionId);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'webui-browser-extension': ExtensionElement;
  }
}

customElements.define(ExtensionElement.is, ExtensionElement);
