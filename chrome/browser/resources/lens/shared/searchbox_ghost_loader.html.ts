// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SearchboxGhostLoaderElement} from './searchbox_ghost_loader.js';

export function getHtml(this: SearchboxGhostLoaderElement) {
  return html`<!--_html_template_start_-->
<!-- LINT.IfChange(GhostLoaderText) -->
<div id="content">
  ${this.showContextualSearchboxLoadingState ? html`
    <div id="loadingState" class="status-container">
      <div class="spinner"></div>
      <div class="loader-text">
        <span id="hint-text1" class="hint-text">
          ${this.getGhostLoaderPrimaryMessage()}
        </span>
        <span id="hint-text2" class="hint-text">
          $i18n{searchboxGhostLoaderHintTextSecondary}
        </span>
      </div>
    </div>
    <div class="suggestion-loader-container">
      ${this.getSuggestionItems().map(() => html`
        <div class="ghost-loader-item">
          <cr-searchbox-icon
              class="searchbox-icon"
              match=""
              default-icon="//resources/cr_components/searchbox/icons/search_cr23.svg">
          </cr-searchbox-icon>
          <div class="loading-bar"></div>
        </div>
      `)}
    </div>
    <div id="errorState" class="status-container">
      <div id="errorIconContainer">
        <div id="errorIcon"></div>
      </div>
      <span class="loader-text">$i18n{searchboxGhostLoaderErrorText}</span>
    </div>
  ` : html`
    <div id="hintState" class="status-container">
      <div id="errorIconContainer">
        <div id="errorIcon"></div>
      </div>
      <span class="loader-text">$i18n{searchboxGhostLoaderNoSuggestText}</span>
    </div>
  `}
</div>
<!-- LINT.ThenChange(//chrome/browser/resources/lens/shared/searchbox_ghost_loader.ts:GhostLoaderText) -->
<!--_html_template_end_-->`;
}
