// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '/shared/icon_from_table.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {ExtensionActionInfo} from '/shared/extensions_bar_data_model.mojom-webui.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import {getHtml} from './extension.html.js';
import {ToolbarActionMixin} from './toolbar_action_mixin.js';
import {getCss} from './toolbar_button.css.js';
import {getContextMenuSourceType} from './toolbar_button.js';

const initialState: ExtensionActionInfo = {
  id: '',
  accessibleName: '',
  tooltip: '',
  isVisible: false,
  icon: {handleId: 0n},
};

const ExtensionElementBase = ToolbarActionMixin(CrLitElement, initialState);

export class ExtensionElement extends ExtensionElementBase {
  static get is() {
    return 'webui-toolbar-extension';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  private browserProxy_ = BrowserProxyImpl.getInstance();

  override getElementId(state: ExtensionActionInfo): string {
    return state.id === '' ? 'kExtensionsMenuButtonElementId' :
                             'kToolbarActionViewElementId';
  }

  override getSecondaryElementId(): string {
    return 'ext:' + this.state.id;
  }

  protected onClick_() {
    this.browserProxy_.toolbarUIHandler.executeExtensionAction(this.state.id);
  }

  protected onContextmenu_(e: Event) {
    e.preventDefault();
    this.browserProxy_.toolbarUIHandler.showExtensionContextMenu(
        this.state.id, getContextMenuSourceType(e));
  }

  override getMimeType() {
    return 'application/x-webui-extension-action';
  }

  override getItemId() {
    return this.state.id;
  }

  override isDraggable() {
    return this.state.id !== '';
  }

  override moveItemBy(delta: number) {
    this.browserProxy_.toolbarUIHandler.moveExtensionActionBy(
        this.state.id, delta);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'webui-toolbar-extension': ExtensionElement;
  }
}

customElements.define(ExtensionElement.is, ExtensionElement);
