// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './extension.js';
import './toolbar_divider.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {ExtensionActionInfo} from '/shared/extensions_bar_data_model.mojom-webui.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import {getCss} from './extensions.css.js';
import {getHtml} from './extensions.html.js';
import {ToolbarActionContainerMixin} from './toolbar_action_container_mixin.js';

export type {KeyedActionState as KeyedExtensionState} from './toolbar_action_container_mixin.js';

const initialState: ExtensionActionInfo[] = [];

const ExtensionsElementBase =
    ToolbarActionContainerMixin(CrLitElement, initialState);

export class ExtensionsElement extends ExtensionsElementBase {
  static get is() {
    return 'webui-toolbar-extensions';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  // ToolbarActionContainerMixin override
  override getKey(state: ExtensionActionInfo): string {
    return state.id;
  }

  override moveItem(id: string, index: number) {
    BrowserProxyImpl.getInstance().toolbarUIHandler.moveExtensionAction(
        id, index);
  }

  override moveItemBy(id: string, delta: number) {
    BrowserProxyImpl.getInstance().toolbarUIHandler.moveExtensionActionBy(
        id, delta);
  }

  override getMimeType() {
    return 'application/x-webui-extension-action';
  }

  override getBroadcastChannelName() {
    return 'extension-action-drag';
  }

  override get childTagName() {
    return 'webui-toolbar-extension';
  }

  override isDivider(_key: string): boolean {
    return false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'webui-toolbar-extensions': ExtensionsElement;
  }
}

customElements.define(ExtensionsElement.is, ExtensionsElement);
