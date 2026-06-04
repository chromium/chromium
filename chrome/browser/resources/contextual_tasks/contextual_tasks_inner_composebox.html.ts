// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ContextualTasksInnerComposeboxElement} from './contextual_tasks_inner_composebox.js';

export function getHtml(this: ContextualTasksInnerComposeboxElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
    <ntp-error-scrim id="errorScrim" part="error-scrim"></ntp-error-scrim>
    <div id="composebox" part="composebox"
        @dragenter="${this.dragAndDropHandler_.handleDragEnter}"
        @dragover="${this.dragAndDropHandler_.handleDragOver}"
        @dragleave="${this.dragAndDropHandler_.handleDragLeave}"
        @drop="${this.dragAndDropHandler_.handleDrop}">
      <div id="inputContainer" part="input-container">
        <cr-composebox-input id="composeboxInput"
            exportparts="text-container, icon-container, mirror, input, smart-compose, cancel, action-icon, cancel-icon">
        </cr-composebox-input>
        <cr-composebox-file-inputs id="fileInputs">
          <cr-composebox-file-carousel id="carousel">
          </cr-composebox-file-carousel>
          <cr-composebox-dropdown id="matches" role="listbox">
          </cr-composebox-dropdown>
        </cr-composebox-file-inputs>
      </div>
    </div>
  <!--_html_template_end_-->`;
  // clang-format on
}
