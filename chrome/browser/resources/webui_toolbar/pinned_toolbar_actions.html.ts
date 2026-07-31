// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, repeat} from '//resources/lit/v3_0/lit.rollup.js';
import {PinnedToolbarAction} from '/shared/toolbar_ui_api_data_model.mojom-webui.js';

import type {PinnedToolbarActionsElement} from './pinned_toolbar_actions.js';

export function getHtml(this: PinnedToolbarActionsElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
${repeat(
    this.keyedStates,
    (keyedState) => keyedState.key,
    (keyedState) => html`
      ${keyedState.state.action === PinnedToolbarAction.kDivider ? html`
        <toolbar-divider
            class="${keyedState.animateIn ? 'animate-in' : ''}
                   ${keyedState.exiting ? 'exiting' : ''}"
            data-key="${keyedState.key}">
        </toolbar-divider>
      ` : html`
        <pinned-toolbar-action
            .state="${keyedState.state}"
            class="${keyedState.animateIn ? 'animate-in' : ''}
                   ${keyedState.exiting ? 'exiting' : ''}
                   ${keyedState.dragPlaceholder ? 'drag-placeholder' : ''}"
            data-key="${keyedState.key}"
            @dragover="${this.onActionDragover}"
            @drop="${this.onActionDrop}">
        </pinned-toolbar-action>
      `}
    `,
)}
<!--_html_template_end_-->`;
  // clang-format on
}
