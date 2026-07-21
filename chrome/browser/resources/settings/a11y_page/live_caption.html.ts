// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SettingsLiveCaptionElement} from './live_caption.js';

export function getHtml(this: SettingsLiveCaptionElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
${!this.enableLiveCaptionMultiLanguage_ ? html`
  <div class="cr-row cr-row-with-template first">
    <settings-toggle-button id="liveCaptionToggleButton"
        pref-key="accessibility.captions.live_caption_enabled"
        @change="${this.onLiveCaptionEnabledChange_}"
        label="$i18n{captionsEnableLiveCaptionTitle}"
        .subLabel="${this.enableLiveCaptionSubtitle_}">
    </settings-toggle-button>
  </div>
  <cr-collapse ?opened="${this.isLiveCaptionEnabled_}">
    <settings-toggle-button id="maskOffensiveWordsToggleButton"
        pref-key="accessibility.captions.live_caption_mask_offensive_words"
        @change="${this.onLiveCaptionMaskOffensiveWordsChange_}"
        label="$i18n{captionsMaskOffensiveWordsTitle}">
    </settings-toggle-button>
  </cr-collapse>
` : ''}
<if expr="not is_chromeos">
${this.enableLiveCaptionMultiLanguage_ ? html`
  <div class="cr-row cr-row-with-template first">
    <settings-toggle-button id="liveCaptionToggleButton"
        pref-key="accessibility.captions.live_caption_enabled"
        @change="${this.onLiveCaptionEnabledChange_}"
        label="$i18n{captionsEnableLiveCaptionTitle}"
        sub-label="$i18n{captionsEnableLiveCaptionSubtitle}">
    </settings-toggle-button>
  </div>
  <cr-collapse ?opened="${this.isLiveCaptionEnabled_}">
    <div class="cr-row continuation">
      <div class="flex settings-box-text">
        $i18n{captionsManageLanguagesTitle}
        <div class="secondary">$i18n{captionsManageLanguagesSubtitle}</div>
      </div>
      <cr-button id="addLanguage" @click="${this.onAddLanguagesClick_}">
        $i18n{addLanguages}
      </cr-button>
    </div>
    <div class="list-frame">
      <div id="languageList" class="vertical-list" role="list">
        ${this.installedLanguagePacks_.map(item => html`
          <div class="list-item" role="listitem">
            <div class="start cr-padded-text">${item.displayName}
              <span id="defaultLanguageLabel"
                  ?hidden="${!this.isDefaultLanguage_(item.code)}">
                $i18n{defaultLanguageLabel}
              </span>
            </div>
            <span aria-live="polite" role="region"
                class="cr-secondary-text cr-row-gap">
              ${item.downloadProgress}
            </span>
            <cr-icon-button class="icon-more-vert" title="$i18n{moreActions}"
                id="more-${item.code}"
                data-code="${item.code}"
                @click="${this.onDotsClick_}"
                aria-label="${this.computeMoreButtonAriaLabel_(
                    item.displayName, item.code)}">
            </cr-icon-button>
          </div>
        `)}
      </div>
    </div>
    <settings-toggle-button id="maskOffensiveWordsToggleButton"
        pref-key="accessibility.captions.live_caption_mask_offensive_words"
        @change="${this.onLiveCaptionMaskOffensiveWordsChange_}"
        label="$i18n{captionsMaskOffensiveWordsTitle}">
    </settings-toggle-button>
  </cr-collapse>
  ${this.enableLiveTranslate_ ? html`
    <settings-live-translate></settings-live-translate>
  ` : ''}
` : ''}
${this.showAddLanguagesDialog_ ? html`
  <settings-add-languages-dialog id="addLanguagesDialog"
      .languages="${this.filterAvailableLanguagePacks_()}"
      @close="${this.onAddLanguagesDialogClose_}"
      @languages-added="${this.onLanguagesAdded_}">
  </settings-add-languages-dialog>
` : ''}
<cr-lazy-render-lit id="menu"
    .template="${() => html`
      <cr-action-menu role-description="$i18n{menu}">
        <button class="dropdown-item" role="menuitem" id="make-default-button"
            @click="${this.onMakeDefaultClick_}">
          $i18n{makeDefaultLanguageLabel}
        </button>
        <button class="dropdown-item" role="menuitem" id="remove-button"
            @click="${this.onRemoveLanguageClick_}">
          $i18n{removeLanguageLabel}
        </button>
      </cr-action-menu>
    `}">
</cr-lazy-render-lit>
</if>
<!--_html_template_end_-->`;
  // clang-format on
}
