// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrInfiniteListElement} from '//resources/cr_elements/cr_infinite_list/cr_infinite_list.js';
import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {StartupPageInfo} from './startup_urls_page_browser_proxy.js';
import type {SettingsStartupUrlsPageElement} from './startup_urls_page.js';

export interface TemplatizedDomNodes {
  infiniteList: CrInfiniteListElement<StartupPageInfo>;
}

export function getHtml(this: SettingsStartupUrlsPageElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="outer" class="flex list-frame">
  <div id="container" class="scroll-container cr-scrollable" scrollable>
    <div class="cr-scrollable-top"></div>
    <cr-infinite-list id="infiniteList" class="cr-separators"
        .items="${this.startupPages_}"
        .scrollTarget="${this.scrollTarget_}"
        .template="${(item: StartupPageInfo, index: number, tabindex: number) => html`
          <settings-startup-url-entry
              .model="${item}"
              tabindex="${tabindex}"
              .focusRowIndex="${index}"
              .listTabIndex="${tabindex}"
              .lastFocused="${this.lastFocused_}"
              @last-focused-changed="${this.onLastFocusedChanged_}"
              .listBlurred="${this.listBlurred_}"
              @list-blurred-changed="${this.onListBlurredChanged_}"
              ?editable="${this.shouldAllowUrlsEdit_()}">
          </settings-startup-url-entry>
        `}">
    </cr-infinite-list>
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
