// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {OverflowMenuItem} from '/shared/toolbar_ui_api.mojom-webui.js';

import type {OverflowableToolbarAction} from './overflowable_toolbar_action_container_mixin.js';
import {getCss} from './toolbar_divider.css.js';
import {getHtml} from './toolbar_divider.html.js';

export class ToolbarDividerElement extends CrLitElement implements
    OverflowableToolbarAction {
  static get is() {
    return 'toolbar-divider';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  isDivider(): boolean {
    return true;
  }

  getOverflowMenuItem(): OverflowMenuItem {
    assertNotReached('Divider does not have overflow menu item');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'toolbar-divider': ToolbarDividerElement;
  }
}

customElements.define(ToolbarDividerElement.is, ToolbarDividerElement);
