// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ViewerSaveToDriveBubbleElement} from './viewer_save_to_drive_bubble.js';

// TODO(crbug.com/427451594): Hook up the buttons to fire events.
export function getHtml(this: ViewerSaveToDriveBubbleElement) {
  return html`<!--_html_template_start_-->
  <dialog id="dialog" @close="${this.onDialogClose_}"
      @focusout="${this.onFocusout_}">
    <div id="header">
      <h2>Save to drive dialog ${this.fileName}</h2>
      <cr-icon-button id="close" iron-icon="cr:close"
          aria-label="$i18n{propertiesDialogClose}"
          title="$i18n{propertiesDialogClose}"
          @click="${this.onCloseClick_}">
      </cr-icon-button>
    </div>
  </dialog>
<!--_html_template_end_-->`;
}
