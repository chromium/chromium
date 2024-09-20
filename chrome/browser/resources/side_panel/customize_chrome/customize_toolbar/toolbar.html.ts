// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ToolbarElement} from './toolbar.js';

export function getHtml(this: ToolbarElement) {
  return html`<!--_html_template_start_-->
<div class="sp-card">
  <sp-heading id="heading" @back-button-click="${this.onBackClick_}"
      back-button-aria-label="$i18n{backButton}"
      back-button-title="$i18n{backButton}">
    <h2 slot="heading">$i18n{toolbarHeader}</h2>
  </sp-heading>
  <div id="miniToolbarBackground">
    <svg id="miniToolbar" src="icons/mini_toolbar.svg">
      <use href="icons/mini_toolbar.svg#miniToolbar"></use>
    </svg>
  </div>
  <div class="intro-text">$i18n{chooseToolbarIconsLabel}</div>
  <cr-button id="resetToDefaultButton" class="floating-button"
      @click="${this.onResetToDefaultClicked_}"
      ?disabled="${this.resetToDefaultDisabled_}">
    <div id="resetToDefaultIcon" class="cr-icon" slot="prefix-icon"></div>
    $i18n{resetToDefaultButtonLabel}
  </cr-button>
</div>
<hr class="sp-cards-separator">
<div class="sp-card" id="pinningSelectionCard">
  ${
      this.categories_.map(
          (category, categoryIndex) => html`
    <h3 class="choose-icons-row category-title">${category.displayName}</h3>
    ${
              this.actions_.map(
                  (action) => action.category === category.id ?
                      html`
      <div
        class="toggle-container choose-icons-row"
        @click="${this.getActionToggleHandler_(action.id, !action.pinned)}"
      >
        <img class="toggle-icon" src="${action.iconUrl.url}"
            aria-hidden="true"></img>
        <div class="toggle-title">${action.displayName}</div>
        <cr-toggle @change="${
                          this.getActionToggleHandler_(
                              action.id, !action.pinned)}"
            ?checked="${action.pinned}" aria-label="${
                          action.displayName}"></cr-toggle>
      </div>
    ` :
                      '')}
    ${
              categoryIndex !== this.categories_.length - 1 ?
                  html`<hr class="sp-hr">` :
                  ''}
  `)}
</div>
<hr class="sp-cards-separator">
<div class="sp-card" id="tipCard">
  <svg id="tipIcon" src="icons/lightbulb_outline.svg">
    <use href="icons/lightbulb_outline.svg#lightbulbOutline"></use>
  </svg>
  $i18n{reorderTipLabel}
</div>
<!--_html_template_end_-->`;
}
