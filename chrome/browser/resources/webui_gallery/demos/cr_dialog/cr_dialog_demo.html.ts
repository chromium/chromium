// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CrDialogDemoElement} from './cr_dialog_demo.js';

export function getHtml(this: CrDialogDemoElement) {
  // clang-format off
  return html`
<h1>cr-dialog</h1>
<div class="demos">
  <cr-checkbox ?checked="${this.showHeader_}"
      @checked-changed="${this.onShowHeaderChanged_}">Show header</cr-checkbox>
  <cr-checkbox ?checked="${this.showFooter_}"
      @checked-changed="${this.onShowFooterChanged_}">Show footer</cr-checkbox>
  <cr-checkbox ?checked="${this.showScrollingBody_}"
      @checked-changed="${this.onShowScrollingBodyChanged_}">
    Show tall scrolling body
  </cr-checkbox>
  <cr-checkbox ?checked="${this.showInputs_}"
      @checked-changed="${this.onShowInputsChanged_}">
    Show inputs
  </cr-checkbox>
  <cr-checkbox ?checked="${this.autofocusInput_}"
      @checked-changed="${this.onAutofocusInputChanged_}"
      ?disabled="${!this.showInputs_}">
    Autofocus input when dialog opens
  </cr-checkbox>

  <cr-button @click="${this.openDialog_}">Open dialog</cr-button>
  <div>
    ${this.statusTexts_.map(item => html`
      <div>${item}</div>
    `)}
  </div>
</div>

${this.isDialogOpen_ ? html`
  <cr-dialog
      id="dialog"
      @cr-dialog-open="${this.onOpenDialog_}"
      @cancel="${this.onCancelDialog_}"
      @close="${this.onCloseDialog_}"
      show-on-attach>
    <div slot="title">Dialog title</div>
    <div slot="header" ?hidden="${!this.showHeader_}">
      Dialogs can also include a header between the title and the body. It is
      commonly used to display status updates or tabs.
    </div>
    <div slot="body">
      <div>Here is where some description text would go.</div>
      <div ?hidden="${!this.showInputs_}">
        <cr-input label="Example input" ?autofocus="${this.autofocusInput_}">
        </cr-input>
        <cr-input label="Example input"></cr-input>
      </div>
      <div ?hidden="${!this.showScrollingBody_}">
        <div id="tallBlock"></div>
      </div>
    </div>
    <div slot="button-container">
      <cr-button class="cancel-button" @click="${this.onClickCancel_}">
        Cancel
      </cr-button>
      <cr-button class="action-button" @click="${this.onClickConfirm_}">
        Confirm
      </cr-button>
    </div>
    <div slot="footer" ?hidden="${!this.showFooter_}">
      Dialogs also have a slot for text or other elements in the footer.
    </div>
  </cr-dialog>
` : ''}`;
  // clang-format on
}
