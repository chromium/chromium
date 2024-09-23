// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {FileHandlingItemElement} from './file_handling_item.js';

export function getHtml(this: FileHandlingItemElement) {
  return html`<!--_html_template_start_-->
<div id="file-handling-item">
  <app-management-toggle-row
     id="toggle-row"
     label="$i18n{appManagementFileHandlingHeader}"
     ?managed="${this.isManaged_()}"
     ?value="${this.getValue_()}"
     class="header-text">
  </app-management-toggle-row>
  <p>
    <localized-link id="type-list"
      .localizedString="${this.userVisibleTypesLabel_()}"
      @link-clicked="${this.launchDialog_}">
    </localized-link>
  </p>
  <localized-link id="learn-more"
    .localizedString="${this.i18nAdvanced('fileHandlingSetDefaults')}"
    .linkUrl="${this.getLearnMoreLinkUrl_()}"
    @link-clicked="${this.onLearnMoreLinkClicked_}">
  </localized-link>
</div>
${this.showOverflowDialog ? html`
  <cr-dialog id="dialog" show-on-attach
      @close="${this.onDialogClose_}">
    <div slot="title">$i18n{fileHandlingOverflowDialogTitle}</div>
    <div id="dialog-body" slot="body">
      ${this.userVisibleTypes_()}
    </div>
    <div slot="button-container">
      <cr-button class="action-button" @click="${this.onCloseButtonClicked_}">
        $i18n{close}
      </cr-button>
    </div>
  </cr-dialog>
` : ''}
<!--_html_template_end_-->`;
}
