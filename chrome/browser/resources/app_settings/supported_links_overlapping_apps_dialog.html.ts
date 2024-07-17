// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SupportedLinksOverlappingAppsDialogElement} from './supported_links_overlapping_apps_dialog.js';

export function getHtml(this: SupportedLinksOverlappingAppsDialogElement) {
  return html`<!--_html_template_start_-->
<cr-dialog show-on-attach id="dialog" close-text="close">
  <div slot="title">$i18n{appManagementIntentOverlapDialogTitle}</div>
  <div slot="body">${this.getBodyText_()}</div>
  <div slot="button-container">
    <cr-button class="cancel-button" @click="${this.onCancelClick_}"
        id="cancel">
      $i18n{cancel}
    </cr-button>
    <cr-button class="action-button" @click="${this.onChangeClick_}"
        id="change">
      $i18n{appManagementIntentOverlapChangeButton}
    </cr-button>
  </div>
</cr-dialog>
<!--_html_template_end_-->`;
}
