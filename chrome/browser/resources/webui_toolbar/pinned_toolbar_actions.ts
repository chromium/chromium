// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './pinned_toolbar_action.js';
import './toolbar_divider.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import {PinnedToolbarAction} from '/shared/toolbar_ui_api_data_model.mojom-webui.js';
import type {PinnedToolbarActionState} from '/shared/toolbar_ui_api_data_model.mojom-webui.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import {OverflowableToolbarActionContainerMixin} from './overflowable_toolbar_action_container_mixin.js';
import type {OverflowableToolbarAction} from './overflowable_toolbar_action_container_mixin.js';
import {getHtml} from './pinned_toolbar_actions.html.js';
import {getCss} from './toolbar_action_container.css.js';
import {ToolbarActionContainerMixin} from './toolbar_action_container_mixin.js';

const initialState: PinnedToolbarActionState[] = [];

const PinnedToolbarActionsElementBase = OverflowableToolbarActionContainerMixin(
    ToolbarActionContainerMixin(CrLitElement, initialState));

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

  static override get properties() {
    return {
      dividerIndex: {type: Number},
    };
  }

  /**
   * The index of the divider within `keyedStates`, or -1 if there is no
   * divider. Updated by onKeyedStatesChanged().
   */
  accessor dividerIndex: number = -1;

  override onKeyedStatesChanged() {
    super.onKeyedStatesChanged();
    // `dividerIndex` has to be updated on keyed states changes, after the
    // ToolbarActionContainerMixin has called reconcileKeys(), but before it has
    // counted its draggable items, so it needs to be updated here, in its own
    // callback in the middle of ToolbarActionContainerMixin.onWillUpdate(),
    // rather that being updated here, rather than in this class's
    // onWillUpdate() method. Moreover, since reconcileKeys() isn't the only
    // method that updates keys, when animations are enabled, we can't override
    // reconcileKeys() and do the update there.
    this.dividerIndex = this.keyedStates.findIndex(
        s => s.key === PinnedToolbarAction.kDivider.toString());
  }

  override getActions(): Array<CrLitElement&OverflowableToolbarAction> {
    return Array.from(this.shadowRoot.children) as
        Array<CrLitElement&OverflowableToolbarAction>;
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

  override isDraggable(state: PinnedToolbarActionState, index: number):
      boolean {
    // Only pinned actions, which precede the divider, may be dragged. If there
    // is no divider, all actions are popped out, so none are draggable.
    if (index >= this.dividerIndex) {
      return false;
    }
    return state.enabled;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'pinned-toolbar-actions': PinnedToolbarActionsElement;
  }
}

customElements.define(
    PinnedToolbarActionsElement.is, PinnedToolbarActionsElement);
