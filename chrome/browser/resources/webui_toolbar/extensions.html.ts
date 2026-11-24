// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, repeat} from '//resources/lit/v3_0/lit.rollup.js';

import type {ExtensionsElement} from './extensions.js';

export function getHtml(this: ExtensionsElement) {
  return html`<!--_html_template_start_-->
${repeat(this.keyedStates, (item) => item.key, (item) => html`
  <webui-toolbar-extension .state="${item.state}"
      class="${item.animateIn ? 'animate-in' : ''}
             ${item.exiting ? 'exiting' : ''}
             ${item.dragPlaceholder ? 'drag-placeholder' : ''}"
      data-key="${item.key}"
      ?extension-action="${this.isExtensionAction(item.state)}"
      @dragover="${this.onActionDragover}" @drop="${this.onActionDrop}">
  </webui-toolbar-extension>
`)}
${this.keyedStates.length > 0 ? html`
  <toolbar-divider
      class="${this.animateInDivider() ? 'animate-in' : ''}
             ${this.allExiting() ? 'exiting' : ''}">
  </toolbar-divider>
` : ''}
<!--_html_template_end_-->`;
}
