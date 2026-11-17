// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SharePasswordNotFamilyMemberDialogElement} from './share_password_not_family_member_dialog.js';

export function getHtml(this: SharePasswordNotFamilyMemberDialogElement) {
  return html`<!--_html_template_start_-->
<cr-dialog id="dialog" close-text="$i18n{close}" show-on-attach>
  <div slot="title">
    <share-password-dialog-header id="header">
      ${this.dialogTitle}
    </share-password-dialog-header>
  </div>
  <div slot="body">
    <picture>
      <source srcset="./images/password_sharing_family_banner_dark.svg"
          media="(prefers-color-scheme: dark)">
      <img src="./images/password_sharing_family_banner.svg"
          role="presentation">
    </picture>
    <div id="description" @click="${this.onDescriptionClick_}"
        .innerHTML="${this.i18nAdvanced('sharePasswordNotFamilyMember')}">
    </div>
  </div>
  <div slot="button-container">
    <cr-button class="action-button" @click="${this.onActionButtonClick_}"
        id="action">
      $i18n{sharePasswordGotIt}
    </cr-button>
  </div>
</cr-dialog>
<!--_html_template_end_-->`;
}
