// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {WallpaperSearchElement} from './wallpaper_search.js';

export function getHtml(this: WallpaperSearchElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="sp-card">
  <sp-heading id="heading" @back-button-click="${this.onBackClick_}"
      back-button-aria-label="$i18n{backButton}"
      back-button-title="$i18n{backButton}">
    <h2 slot="heading">$i18n{wallpaperSearchPageHeader}</h2>
  </sp-heading>
  ${this.errorState_ !== null ? html`
    <div class="content" id="error" tabindex="-1">
      <div id="errorTitle">${this.errorState_.title}</div>
      <div id="errorDescription">${this.errorState_.description}</div>
      <cr-button id="errorCTA" @click="${this.onErrorClick_}">
        ${this.errorState_.callToAction}
      </cr-button>
    </div>
  ` : ''}
  <div class="content" id="wallpaperSearch" ?hidden="${this.errorState_}"
      tabindex="-1">
    <div role="application">
      <customize-chrome-combobox id="descriptorComboboxA"
          label="$i18n{wallpaperSearchSubjectLabel}"
          default-option-label="$i18n{wallpaperSearchSubjectDefaultOptionLabel}"
          .items="${this.comboboxItems_.a}"
          .value="${this.selectedDescriptorA_}"
          @value-changed="${this.onSubjectDescriptorChange_}">
      </customize-chrome-combobox>
      <div id="optionalDetails">
        <div id="optionalDetailsLabel">$i18n{optionalDetailsLabel}</div>
        <customize-chrome-combobox id="descriptorComboboxB"
            label="$i18n{wallpaperSearchStyleLabel}"
            default-option-label="$i18n{wallpaperSearchStyleDefaultOptionLabel}"
            .items="${this.comboboxItems_.b}"
            .value="${this.selectedDescriptorB_}"
            @value-changed="${this.onStyleDescriptorChange_}">
        </customize-chrome-combobox>
        <customize-chrome-combobox id="descriptorComboboxC"
            label="$i18n{wallpaperSearchMoodLabel}"
            default-option-label="$i18n{wallpaperSearchMoodDefaultOptionLabel}"
            .items="${this.comboboxItems_.c}"
            .value="${this.selectedDescriptorC_}" right-align-dropbox
            @value-changed="${this.onMoodDescriptorChange_}">
        </customize-chrome-combobox>
        <cr-grid columns="6" id="descriptorMenuD" role="radiogroup">
          ${this.descriptorD_.map((item, index) => html`
            <button class="default-color"
                data-index="${index}" @click="${this.onDefaultColorClick_}"
                aria-checked="${this.isColorSelected_(item)}"
                title="${this.getColorLabel_(item)}">
              <cr-ripple></cr-ripple>
              <customize-chrome-check-mark-wrapper class="color-check-mark"
                  ?checked="${this.isColorSelected_(item)}"
                  checkmark-border-hidden>
                <span class="descriptor-d" .style="background-color: ${item};">
                </span>
              </customize-chrome-check-mark-wrapper>
            </button>
          `)}
          <button id="customColorContainer" @click="${this.onCustomColorClick_}"
              aria-checked="${this.isCustomColorSelected_()}"
              title="$i18n{colorPickerLabel}">
            <cr-ripple></cr-ripple>
            <customize-chrome-check-mark-wrapper class="color-check-mark"
                ?checked="${this.isCustomColorSelected_()}"
                checkmark-border-hidden>
              <div class="descriptor-d"
                  .style="background: hsl(${this.selectedHue_}, 100%, 50%);">
                <div id="colorPickerIcon"></div>
              </div>
            </customize-chrome-check-mark-wrapper>
          </button>
        </div>
      </cr-grid>
      <cr-theme-hue-slider-dialog id="hueSlider"
          @selected-hue-changed="${this.onSelectedHueChanged_}">
        <cr-icon-button slot="headerSuffix" id="deleteSelectedHueButton"
            ?hidden="${!this.shouldShowDeleteSelectedHueButton_()}"
            title="$i18n{hueSliderDeleteTitle}"
            aria-label="$i18n{hueSliderDeleteA11yLabel}"
            @click="${this.onSelectedHueDelete_}">
        </cr-icon-button>
      </cr-theme-hue-slider-dialog>
      <div id="btnContainer">
        <cr-button
            id="submitButton"
            @click="${this.onSearchClick_}"
            class="action-button">
          <div id="imageIcon" class="cr-icon" slot="prefix-icon"></div>
          $i18n{wallpaperSearchSubmitBtn}
        </cr-button>
      </div>
    </div>
    <hr class="sp-hr">
    <div id="loading" ?hidden="${!this.loading_}">
      <cr-loading-gradient>
        <svg width="280" height="183" xmlns="http://www.w3.org/2000/svg">
          <clipPath>
            <rect x="0" y="0" width="86.67" height="86.67" rx="12"></rect>
            <rect x="96.67" y="0" width="86.67" height="86.67" rx="12"></rect>
            <rect x="193.34" y="0" width="86.67" height="86.67" rx="12"></rect>
            <rect x="0" y="96.67" width="86.67" height="86.67" rx="12"></rect>
            <rect x="96.67" y="96.67" width="86.67" height="86.67" rx="12">
            </rect>
            <rect x="193.34" y="96.67" width="86.67" height="86.67" rx="12">
            </rect>
          </clipPath>
        </svg>
      </cr-loading-gradient>
    </div>
    <cr-grid columns="3" disable-arrow-navigation
        ?hidden="${!this.shouldShowGrid_()}" role="radiogroup">
      ${this.results_.map((item, index) => html`
        <div class="tile result" tabindex="0" role="radio"
            data-index="${index}" @click="${this.onResultClick_}"
            aria-label="${this.getResultAriaLabel_(index)}"
            aria-checked="${this.isBackgroundSelected_(item.id)}">
          <customize-chrome-check-mark-wrapper class="image-check-mark"
              ?checked="${this.isBackgroundSelected_(item.id)}">
            <div class="image-container">
              <img src="data:image/png;base64,${item.image}">
              </img>
            </div>
          </customize-chrome-check-mark-wrapper>
        </div>
      `)}
      ${this.emptyResultContainers_.map(_ => html`
        <div class="tile empty">
          <div class="image-container"></div>
        </div>
      `)}
    </cr-grid>
    <div id="footer">
      <div id="disclaimer">
        $i18n{experimentalFeatureDisclaimer}
        <a href="#" aria-label="$i18n{learnMoreAboutFeatureA11yLabel}"
            @click="${this.onLearnMoreClick_}">$i18n{learnMore}</a>
      </div>
      <div ?hidden="${!this.loading_}">
        <cr-loading-gradient>
          <svg height="16" width="44" xmlns="http://www.w3.org/2000/svg">
            <clipPath>
              <circle cx="8" cy="8" r="8"></circle>
              <circle cx="36" cy="8" r="8"></circle>
            </clipPath>
          </svg>
        </cr-loading-gradient>
      </div>
      <cr-feedback-buttons id="feedbackButtons"
          ?hidden="${!this.shouldShowFeedbackButtons_()}"
          .selectedOption="${this.selectedFeedbackOption_}"
          @selected-option-changed="${this.onFeedbackSelectedOptionChanged_}">
      </cr-feedback-buttons>
    </div>
  </div>
</div>
${this.inspirationCardEnabled_ ? html`
  <hr class="sp-cards-separator" ?hidden="${!this.shouldShowInspiration_}">
  <div class="sp-card" id="inspirationCard"
      ?hidden="${!this.shouldShowInspiration_}">
    <sp-heading hide-back-button role="button"
        aria-label="$i18n{showInspirationCardToggle}"
        title="$i18n{showInspirationCardToggle}"
        @click="${this.onInspirationToggleClick_}"
        @keydown="${this.onButtonKeydown_}"
        aria-expanded="${this.openInspirations_}"
        tabindex="0"
        id="inspirationToggle">
      <h2 slot="heading">$i18n{wallpaperSearchInspirationHeader}</h2>
      <div class="cr-icon ${this.inspirationToggleIcon_}" slot="buttons">
      </div>
    </sp-heading>
    <cr-collapse .opened="${this.openInspirations_}">
      <div class="inspirations-content">
        ${this.inspirationGroups_.map((item, groupIndex) => html`
          <h3>
            <div class="inspiration-title" role="button" tabindex="0"
                data-index="${groupIndex}"
                @click="${this.onInspirationGroupTitleClick_}"
                @keydown="${this.onButtonKeydown_}"
                aria-current="${
                      this.getInspirationDescriptorsCheckedStatus_(
                          item.descriptors)}">
              ${this.getInspirationGroupTitle_(item.descriptors)}
            </div>
          </h3>
          <cr-grid columns="3" disable-arrow-navigation role="radiogroup">
            ${item.inspirations.map((item, index) => html`
              <div class="tile result"
                  data-group-index="${groupIndex}" data-index="${index}"
                  @click="${this.onInspirationImageClick_}"
                  tabindex="0" role="radio"
                  aria-checked="${this.isBackgroundSelected_(item.id)}"
                  aria-label="${item.description}">
                <customize-chrome-check-mark-wrapper class="image-check-mark"
                    ?checked="${this.isBackgroundSelected_(item.id)}">
                  <div class="image-container">
                    <img is="cr-auto-img" .autoSrc="${item.thumbnailUrl.url}">
                  </div>
                </customize-chrome-check-mark-wrapper>
              </div>
            `)}
          </cr-grid>
        `)}
      </div>
    </cr-collapse>
  </div>
` : ''}
<hr class="sp-cards-separator" ?hidden="${!this.shouldShowHistory_}">
<div class="sp-card" id="historyCard" ?hidden="${!this.shouldShowHistory_}">
  <sp-heading hide-back-button>
    <h2 slot="heading">$i18n{wallpaperSearchHistoryHeader}</h2>
  </sp-heading>
  <div class="content">
    <cr-grid columns="3" disable-arrow-navigation role="radiogroup">
      ${this.history_.map((item, index) => html`
        <div class="tile result" tabindex="0" role="radio"
            aria-label="${this.getHistoryResultAriaLabel_(index, item)}"
            data-index="${index}" @click="${this.onHistoryImageClick_}"
            aria-checked="${this.isBackgroundSelected_(item.id)}">
          <customize-chrome-check-mark-wrapper class="image-check-mark"
              ?checked="${this.isBackgroundSelected_(item.id)}">
            <div class="image-container">
              <img src="data:image/png;base64,${item.image}">
              </img>
            </div>
          </customize-chrome-check-mark-wrapper>
        </div>
      `)}
      ${this.emptyHistoryContainers_.map(_ => html`
        <div class="tile empty">
          <div class="image-container"></div>
        </div>
      `)}
    </cr-grid>
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
