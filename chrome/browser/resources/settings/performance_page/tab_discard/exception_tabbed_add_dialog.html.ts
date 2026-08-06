// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';
import type {ExceptionTabbedAddDialogElement} from './exception_tabbed_add_dialog.js';

export function getHtml(this: ExceptionTabbedAddDialogElement) {
  return html`<!--_html_template_start_-->
<cr-dialog id="dialog" close-text="$i18n{close}">
  <div slot="title">$i18n{addSitesTitle}</div>
  <div slot="header">
    <cr-tabs id="tabs" .tabNames="${this.tabNames_}"
        .selected="${this.selectedTab_}"
        @selected-changed="${this.onTabsSelectedChanged_}">
    </cr-tabs>
  </div>
  <div id="body" slot="body">
    <cr-page-selector .selected="${this.selectedTab_}">
      <tab-discard-exception-current-sites-list id="list"
          @sites-populated="${this.onSitesPopulated_}"
          .visible="${this.isAddCurrentSitesTabSelected_()}"
          @submit-disabled-changed="${this.onListSubmitDisabledChanged_}">
      </tab-discard-exception-current-sites-list>
      <div id="inputPage">
        <div id="helpText">
          $i18nRaw{tabDiscardingExceptionsAddDialogHelp}
        </div>
        <tab-discard-exception-add-input id="input"
            @submit-disabled-changed="${this.onInputSubmitDisabledChanged_}">
        </tab-discard-exception-add-input>
      </div>
    </cr-page-selector>
  </div>
  <div slot="button-container">
    <cr-button id="cancelButton" class="cancel-button"
        @click="${this.onCancelClick_}">
      $i18n{cancel}
    </cr-button>
    <cr-button id="actionButton" class="action-button"
        @click="${this.onSubmitClick_}" ?disabled="${this.isSubmitDisabled_()}"
        aria-label="$i18n{tabDiscardingExceptionsAddButtonAriaLabel}">
      $i18n{add}
    </cr-button>
  </div>
</cr-dialog>
<!--_html_template_end_-->`;
}
