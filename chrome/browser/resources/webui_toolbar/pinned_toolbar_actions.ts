// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './pinned_toolbar_action.js';
import './toolbar_divider.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import {PinnedToolbarAction} from '/shared/toolbar_ui_api_data_model.mojom-webui.js';
import type {PinnedToolbarActionState} from '/shared/toolbar_ui_api_data_model.mojom-webui.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import {getHtml} from './pinned_toolbar_actions.html.js';
import {getCss} from './toolbar_action_container.css.js';
import {ToolbarActionContainerMixin} from './toolbar_action_container_mixin.js';

export type {KeyedActionState} from './toolbar_action_container_mixin.js';

const initialState: PinnedToolbarActionState[] = [];

const PinnedToolbarActionsElementBase =
    ToolbarActionContainerMixin(CrLitElement, initialState);

export class PinnedToolbarActionsElement extends
    PinnedToolbarActionsElementBase {
  static get is() {
    return 'pinned-toolbar-actions';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  // ToolbarActionContainerMixin override
  override getKey(state: PinnedToolbarActionState): string {
    return state.action.toString();
  }

  // ToolbarActionContainerMixin override
  override isInitialUpdate(newStates: PinnedToolbarActionState[]): boolean {
    return (!this.keyedStates || this.keyedStates.length === 0) &&
        // Initial updates contain only pinned items, which requires a divider.
        newStates.some(s => s.action === PinnedToolbarAction.kDivider);
  }

  override moveItem(id: string, index: number) {
    const actionId = parseInt(id, 10) as PinnedToolbarAction;
    BrowserProxyImpl.getInstance().toolbarUIHandler.movePinnedToolbarAction(
        actionId, index);
  }

  override moveItemBy(id: string, delta: number) {
    const actionId = parseInt(id, 10) as PinnedToolbarAction;
    BrowserProxyImpl.getInstance().toolbarUIHandler.movePinnedToolbarActionBy(
        actionId, delta);
  }

  override getMimeType() {
    return 'application/x-webui-pinned-action';
  }

  override getBroadcastChannelName() {
    return 'pinned-action-drag';
  }

  override get childTagName() {
    return 'pinned-toolbar-action';
  }

  override isDivider(key: string): boolean {
    return key === PinnedToolbarAction.kDivider.toString();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'pinned-toolbar-actions': PinnedToolbarActionsElement;
  }
}

customElements.define(
    PinnedToolbarActionsElement.is, PinnedToolbarActionsElement);
