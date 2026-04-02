// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './pinned_toolbar_action.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './pinned_toolbar_actions.css.js';
import {getHtml} from './pinned_toolbar_actions.html.js';
import type {PinnedToolbarActionState} from './toolbar_ui_api_data_model.mojom-webui.js';

export class PinnedToolbarActionsElement extends CrLitElement {
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
      state: {type: Array},
    };
  }

  protected accessor state: PinnedToolbarActionState[] = [];
}

declare global {
  interface HTMLElementTagNameMap {
    'pinned-toolbar-actions': PinnedToolbarActionsElement;
  }
}

customElements.define(
    PinnedToolbarActionsElement.is, PinnedToolbarActionsElement);
