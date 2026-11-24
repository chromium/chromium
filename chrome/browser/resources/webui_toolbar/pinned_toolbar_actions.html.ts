// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, repeat} from '//resources/lit/v3_0/lit.rollup.js';
import {PinnedToolbarAction} from '/shared/toolbar_ui_api_data_model.mojom-webui.js';

import type {PinnedToolbarActionsElement} from './pinned_toolbar_actions.js';

export function getHtml(this: PinnedToolbarActionsElement) {
  return html`<!--_html_template_start_-->
${repeat(this.keyedStates, (item) => item.key, (item, index) => html`
  ${item.state.action === PinnedToolbarAction.kDivider ? html`
    <toolbar-divider
        class="${item.animateIn ? 'animate-in' : ''}
                   ${item.exiting ? 'exiting' : ''}"
        data-key="${item.key}">
    </toolbar-divider>
  ` : html`
    <pinned-toolbar-action .state="${item.state}"
        .poppedOut="${index > this.dividerIndex}"
        class="${item.animateIn ? 'animate-in' : ''}
                   ${item.exiting ? 'exiting' : ''}
                   ${item.dragPlaceholder ? 'drag-placeholder' : ''}"
        data-key="${item.key}" @dragover="${this.onActionDragover}"
        @drop="${this.onActionDrop}">
    </pinned-toolbar-action>
  `}
`)}
<!--_html_template_end_-->`;
}
