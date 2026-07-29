// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SettingsStartupUrlsPageElement} from './startup_urls_page.js';

export function getHtml(this: SettingsStartupUrlsPageElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="outer" class="flex list-frame">
  <div id="container" class="scroll-container cr-scrollable" scrollable>
    <div class="cr-scrollable-top"></div>
    <div id="list" focusgroup="toolbar block">
      ${this.startupPages_.map(item => html`
        <settings-startup-url-entry
            .model="${item}"
            ?editable="${this.shouldAllowUrlsEdit_()}">
        </settings-startup-url-entry>
      `)}
    </div>
    <div class="cr-scrollable-bottom"></div>
  </div>
</div>
<div id="editOptions" class="list-frame">
  ${this.shouldAllowUrlsEdit_() ? html`
    <div class="list-item" id="addPage">
      <a is="action-link" class="list-button" @click="${this.onAddPageClick_}">
        $i18n{onStartupAddNewPage}
      </a>
    </div>
    <div class="list-item" id="useCurrentPages">
      <a is="action-link" class="list-button"
          @click="${this.onUseCurrentPagesClick_}">
        $i18n{onStartupUseCurrent}
      </a>
    </div>
  ` : ''}
  ${this.startupUrlsPref_?.extensionId ? html`
    <extension-controlled-indicator
        .extensionId="${this.startupUrlsPref_.extensionId}"
        .extensionName="${this.startupUrlsPref_.controlledByName!}"
        .extensionCanBeDisabled="${this.startupUrlsPref_.extensionCanBeDisabled!}">
    </extension-controlled-indicator>
  ` : ''}
</div>
${this.showStartupUrlDialog_ ? html`
  <settings-startup-url-dialog .model="${this.startupUrlDialogModel_}"
      @close="${this.onDialogClose_}">
  </settings-startup-url-dialog>
` : ''}
<!--_html_template_end_-->`;
  // clang-format on
}
