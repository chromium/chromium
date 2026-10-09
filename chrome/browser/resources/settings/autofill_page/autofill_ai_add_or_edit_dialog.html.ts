// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsAutofillAiAddOrEditDialogElement} from './autofill_ai_add_or_edit_dialog.js';

export function getHtml(this: SettingsAutofillAiAddOrEditDialogElement) {
  return html`<!--_html_template_start_-->
<cr-dialog id="dialog" close-text="$i18n{close}"
    @cancel="${this.onDialogCancel_}">
  <div slot="title" class="title-container">
    <div class="title-text">${this.dialogTitle}</div>

    ${this.shouldShowWalletBranding_() ? html`
      <if expr="_google_chrome">
        ${this.shouldShowStandaloneWalletIcon_() ? html`
          <div class="title-icon-container-standalone">
            <img srcset="${this.getScaledSrcSet_('chrome://theme/IDR_AUTOFILL_GOOGLE_WALLET_ICON')}"
                alt=""
                role="presentation">
          </div>
        ` : html`
          <div class="title-icon-container">
            <picture id="branding-image">
              <source srcset="${this.getScaledSrcSet_('chrome://theme/IDR_AUTOFILL_GOOGLE_WALLET_DARK')}"
                  media="(prefers-color-scheme: dark)">
              <img srcset="${this.getScaledSrcSet_('chrome://theme/IDR_AUTOFILL_GOOGLE_WALLET')}"
                  alt=""
                  role="presentation">
            </picture>
          </div>
        `}
      </if>
    ` : ''}
  </div>
  <div slot="body">
    <div id="validation-error-top" class="cr-form-field-label"
        ?hidden="${!this.validationError_}">
      ${this.validationError_}
    </div>

    ${this.completeAttributeInstanceList_.map((attributeInstanceItem, index) => html`
      ${this.isDataType_(attributeInstanceItem,
          chrome.autofillPrivate.AttributeTypeDataType.COUNTRY) ? html`
        <label id="country-select-label" class="cr-form-field-label">
          ${attributeInstanceItem.type.typeNameAsString}${this.getRequiredIndicator_(attributeInstanceItem)}
        </label>
        <select id="country-select" class="md-select"
            aria-label="${attributeInstanceItem.type.typeNameAsString}"
            data-index="${index}"
            @change="${this.onCountrySelectChange_}">
          <option value="">$i18n{autofillDropdownNoOptionSelected}</option>
          ${this.countryList_.map(countryItem => html`
            <option value="${this.getCountryCode_(countryItem)}"
                ?disabled="${this.isCountrySeparator_(countryItem)}"
                ?selected="${this.isCountrySelected_(
                    attributeInstanceItem, countryItem)}">
              ${this.getCountryName_(countryItem)}
            </option>
          `)}
        </select>
      ` : ''}

      <!-- TODO(crbug.com/406006293): Revisit the design of date pickers. -->
      ${this.isDataType_(attributeInstanceItem,
          chrome.autofillPrivate.AttributeTypeDataType.DATE) ? html`
        <div class="date-field">

          <label id="date-select-label"
              class="cr-form-field-label"
              ?hidden="${this.isDateInvalid_(attributeInstanceItem)}">
            ${attributeInstanceItem.type.typeNameAsString}${this.getRequiredIndicator_(attributeInstanceItem)}
          </label>
          <label id="invalid-date-select-label"
              class="cr-form-field-label"
              ?hidden="${!this.isDateInvalid_(attributeInstanceItem)}">
            ${attributeInstanceItem.type.typeNameAsString}${this.getRequiredIndicator_(attributeInstanceItem)}
          </label>

          <select id="month-select" class="md-select"
              aria-label="${this.i18n('autofillAiAccessibilityLabelMonthDropdown',
                  attributeInstanceItem.type.typeNameAsString)}"
              data-index="${index}"
              @change="${this.onMonthSelectChange_}">
            <option value="">
              $i18n{autofillAiMonthDropdownNoOptionSelected}
            </option>
            ${this.months_.map(month => html`
              <option value="${month}"
                  ?selected="${this.isMonthSelected_(attributeInstanceItem, month)}">
                ${this.getMonthName_(month)}
              </option>
            `)}
          </select>

          <select id="day-select" class="md-select"
              aria-label="${this.i18n('autofillAiAccessibilityLabelDayDropdown',
                  attributeInstanceItem.type.typeNameAsString)}"
              data-index="${index}"
              @change="${this.onDaySelectChange_}">
            <option value="">
              $i18n{autofillAiDayDropdownNoOptionSelected}
            </option>
            ${this.days_.map(day => html`
              <option value="${day}"
                  ?selected="${this.isDaySelected_(attributeInstanceItem, day)}">
                ${day}
              </option>
            `)}
          </select>

          <select id="year-select" class="md-select"
              aria-label="${this.i18n('autofillAiAccessibilityLabelYearDropdown',
                  attributeInstanceItem.type.typeNameAsString)}"
              data-index="${index}"
              @change="${this.onYearSelectChange_}">
            <option value="">
              $i18n{autofillAiYearDropdownNoOptionSelected}
            </option>
            ${this.isExistingYearOutOfBounds_(attributeInstanceItem) ? html`
              <option value="${this.getExistingYear_(attributeInstanceItem)}"
                  ?selected="${this.isYearSelected_(
                      attributeInstanceItem,
                      this.getExistingYear_(attributeInstanceItem))}">
                ${this.getExistingYear_(attributeInstanceItem)}
              </option>
            ` : ''}
            <!-- TODO(crbug.com/403312087): Use an <hr> element instead. Resolve
                 this TODO only when an <hr> element is also used for the
                 country selector, for consistency. -->
            <!-- This separator follows the same pattern as the one for
                 the country selector. -->
            <option value="SEPARATOR" disabled>
              ------
            </option>
            ${this.years_.map(year => html`
              <option value="${year}"
                  ?selected="${this.isYearSelected_(attributeInstanceItem, year)}">
                ${year}
              </option>
            `)}
          </select>

          <div id="date-validation-error" class="cr-form-field-label"
              ?hidden="${!this.isDateInvalid_(attributeInstanceItem)}">
            $i18n{autofillAiAddOrEditDialogDateValidationError}
          </div>
        </div>
      ` : ''}

      ${this.isDataType_(attributeInstanceItem,
          chrome.autofillPrivate.AttributeTypeDataType.STRING) ? html`
        <cr-input id="attribute-instance-field" type="text"
            label="${this.computeInputLabel_(attributeInstanceItem)}"
            .value="${attributeInstanceItem.value as string}"
            data-index="${index}"
            spellcheck="false" maxlength="1000"
            @value-changed="${this.onAttributeInstanceFieldValueChanged_}"
            @input="${this.onAttributeInstanceFieldInput_}"
            ?invalid="${this.isFieldInvalid_(attributeInstanceItem)}">
        </cr-input>
      ` : ''}
    `)}
    <div id="footer"
         ?hidden="${this.shouldHideFooterText_()}"
         .innerHTML="${this.footerText_}">
    </div>
  </div>
  <div slot="button-container">
    <cr-button class="cancel-button" @click="${this.onCancelClick_}"
        ?disabled="${this.saveInProgress_}">
      $i18n{cancel}
    </cr-button>
    <cr-button class="action-button" ?disabled="${!this.canSave_}"
        @click="${this.onConfirmClick_}">
      <div class="spinner"
         ?hidden="${!this.shouldShowSpinner_()}">
      </div>

      <span ?hidden="${this.shouldShowSpinner_()}">
        <!--
          When saving a Wallet private pass a consent is recorded that
          includes the title of the confirmation button. Ensure that the correct
          string ID is referenced in the backend code.
        -->
        <!-- LINT.IfChange -->
        $i18n{save}
        <!-- LINT.ThenChange(//chrome/browser/extensions/api/autofill_private/autofill_private_api.cc) -->
      </span>
    </cr-button>
  </div>
</cr-dialog>
<!--_html_template_end_-->`;
}
