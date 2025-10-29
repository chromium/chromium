// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SetupListModuleWrapperElement} from './setup_list_module_wrapper.js';

export function getHtml(this: SetupListModuleWrapperElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="container"
    ?hidden="${this.isModuleHidden_()}"
    @module-ready="${this.onModuleReady_}"
    @disable-module="${this.onHideModule_}"
    @dismiss-module-instance="${this.onHideModule_}">
  <div id="moduleElement">
    <setup-list id="setupList" maxPromos="${this.maxPromos}"
        maxCompletedPromos="${this.maxCompletedPromos}">
    </setup-list>
  </div>
</div>
<cr-toast id="undoToast" duration="10000">
  <div id="undoToastMessage">${this.undoData_?.message || ''}</div>
  ${this.undoData_?.undo ? html `
    <cr-button id="undoButton" aria-label="$i18n{undoDescription}"
        @click="${this.onUndoButtonClick_}">
      $i18n{undo}
    </cr-button>
  ` : ''}
</cr-toast>
<!--_html_template_end_-->`;
  // clang-format on
}
