// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CertificatePasswordDialogElement} from './certificate_password_dialog.js';

export function getHtml(this: CertificatePasswordDialogElement) {
  return html`<!--_html_template_start_-->
    <cr-dialog id="dialog" show-on-attach>
      <div slot="title">
        $i18n{certificateManagerV2EnterPasswordTitle}
      </div>
      <div slot="body">
        <cr-input id="password" type="password" autofocus>
        <!-- TODO(crbug.com/40928765): add a button to toggle the password
            being visible -->
        </cr-input>
      </div>
      <div slot="button-container">
        <cr-button id="cancel" class="cancel-button"
            @click="${this.onCancelClick_}">
          $i18n{cancel}
        </cr-button>
        <cr-button id="ok" class="action-button" @click="${this.onOkClick_}">
          $i18n{ok}
        </cr-button>
      </div>
    </cr-dialog>
    <!--_html_template_end_-->`;
}
