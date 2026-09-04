// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsSpellCheckPageElement} from './spell_check_page.js';

export function getHtml(this: SettingsSpellCheckPageElement) {
  return html`<!--_html_template_start_-->
<settings-section page-title="$i18n{spellCheckTitle}">
  <div route-path="default">
    <settings-toggle-button id="enableSpellcheckingToggle"
        label="$i18n{offerToEnableSpellCheck}"
        .subLabel="${this.getSpellCheckSubLabel_()}"
        pref-key="browser.enable_spellchecking"
        ?disabled="${this.isSpellCheckToggleDisabled_()}"
        @settings-boolean-control-change="${this.onEnableSpellcheckingToggleSettingsBooleanControlChange_}">
    </settings-toggle-button>
<if expr="_google_chrome or not is_macosx">
    <cr-collapse id="spellCheckCollapse"
        ?opened="${!!this.enableSpellcheckingPref_?.value}">
<if expr="_google_chrome">
      <div class="cr-row continuation spell-check-radio-group">
        <settings-radio-group class="flex"
            pref-key="spellcheck.use_spelling_service"
            @change="${this.onSelectedSpellingServiceChange_}">
          <controlled-radio-button class="spell-check-radio-button"
              id="spellingServiceDisable"
              label="$i18n{spellCheckBasicLabel}" name="false"
              pref-key="spellcheck.use_spelling_service">
          </controlled-radio-button>
          <controlled-radio-button class="spell-check-radio-button enhanced"
              id="spellingServiceEnable"
              label="$i18n{spellCheckEnhancedLabel}" name="true"
              pref-key="spellcheck.use_spelling_service">
            <div class="secondary enhanced-spellcheck-description">
              $i18n{spellCheckEnhancedDescription}
            </div>
          </controlled-radio-button>
        </settings-radio-group>
      </div>
</if> <!-- _google_chrome -->
<if expr="not is_macosx">
      <div id="spellCheckLanguagesList"
          ?hidden="${this.shouldHideSpellCheckLanguages_()}">
        <div class="cr-row continuation">
          $i18n{spellCheckLanguagesListTitle}
        </div>
        <div class="list-frame vertical-list spell-check-languages" role="list">
          ${this.spellCheckLanguages_.map((item, index) => html`
            <div class="list-item" role="listitem">
              <div class="start name-with-error-list"
                  @click="${this.onSpellCheckNameClick_}" actionable
                  data-index="${index}"
                  ?disabled="${this.isSpellCheckNameClickDisabled_(item)}">
                ${item.language.displayName}
                <div ?hidden="${!this.errorsGreaterThan_(
                    item.downloadDictionaryFailureCount, 0)}">
                  <cr-icon icon="cr:error-filled"></cr-icon>
                  $i18n{languagesDictionaryDownloadError}
                </div>
                <div ?hidden="${!this.errorsGreaterThan_(
                    item.downloadDictionaryFailureCount, 1)}">
                  $i18n{languagesDictionaryDownloadErrorHelp}
                </div>
              </div>
              <cr-button @click="${this.onRetryDictionaryDownloadClick_}"
                  data-index="${index}"
                  ?hidden="${!this.errorsGreaterThan_(
                      item.downloadDictionaryFailureCount, 0)}">
                $i18n{retry}
              </cr-button>
              ${!item.isManaged ? html`
                <cr-toggle @change="${this.onSpellCheckLanguageChange_}"
                    data-index="${index}"
                    ?disabled="${!item.language.supportsSpellcheck}"
                    ?checked="${item.spellCheckEnabled}"
                    aria-label="${item.language.displayName}"
                    ?hidden="${this.errorsGreaterThan_(
                        item.downloadDictionaryFailureCount, 0)}">
                </cr-toggle>
              ` : html`
                <cr-policy-pref-indicator
                    .pref="${this.getIndicatorPrefForManagedSpellcheckLanguage_(
                        item.spellCheckEnabled)}"
                    ?hidden="${this.errorsGreaterThan_(
                        item.downloadDictionaryFailureCount, 0)}">
                </cr-policy-pref-indicator>
                <cr-toggle disabled
                    ?checked="${item.spellCheckEnabled}"
                    aria-label="${item.language.displayName}"
                    ?hidden="${this.errorsGreaterThan_(
                        item.downloadDictionaryFailureCount, 0)}">
                </cr-toggle>
              `}
            </div>
          `)}
        </div>
      </div>
      <cr-link-row @click="${this.onEditDictionaryClick_}"
          id="spellCheckSubpageTrigger" label="$i18n{manageSpellCheck}"
          role-description="$i18n{subpageArrowRoleDescription}">
      </cr-link-row>
</if> <!-- not is_macosx -->
    </cr-collapse>
</if> <!-- _google_chrome or not is_macosx -->
  </div>
</settings-section>
<!--_html_template_end_-->`;
}
