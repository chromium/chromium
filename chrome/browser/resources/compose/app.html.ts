/* Copyright 2026 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ComposeAppElement} from './app.js';
import {InputMode} from './compose.mojom-webui.js';

export function getHtml(this: ComposeAppElement) {
  // clang-format off
  return html`
<!--_html_template_start_-->
<div id="firstRunDialog" class="dialog"
    ?hidden="${!this.showFirstRunDialog_}">
  <div id="firstRunHeading">
    <div id="firstRunIconContainer">
      <cr-icon icon="compose:compose"></cr-icon>
    </div>
    <h1>$i18n{firstRunTitle}</h1>
    <cr-icon-button id="firstRunCloseButton" class="close-button"
      iron-icon="cr:close" @click="${this.onClose_}"
      aria-label="$i18n{close}">
    </cr-icon-button>
  </div>

  <div id="firstRunContainer">
    <div id="firstRunTopText">$i18n{firstRunMainTop}</div>
    <div id="firstRunMidText" ?hidden="${this.enterprise_}">
        $i18nRaw{firstRunMainMid}</div>
    <div id="firstRunMidTextEnterprise" ?hidden="${!this.enterprise_}">
        $i18nRaw{firstRunMainMidEnterprise}</div>
    <div id="firstRunBottomText" @click="${this.onFirstRunBottomTextClick_}">
      $i18nRaw{firstRunMainBottom}
    </div>
  </div>

  <div id="firstRunFooter" class="footer">
    <cr-button id="firstRunOkButton" class="action-button"
      @click="${this.onFirstRunOkButtonClick_}">
      $i18n{firstRunOkButton}
    </cr-button>
  </div>
</div>

<div id="freMsbbDialog" class="dialog" ?hidden="${!this.showMSBBDialog_}">
  <div id="freMsbbHeading">
    <h1>$i18n{freMsbbTitle}</h1>
    <cr-icon-button id="closeButtonMSBB" class="close-button"
      iron-icon="cr:close" @click="${this.onClose_}">
    </cr-icon-button>
  </div>

  <div id="freMsbbContainer">
    <div id="freMsbbText">
      $i18nRaw{freMsbbMain}
    </div>
  </div>

  <div id="freMsbbFooter" class="footer">
    <cr-button id="SettingsButton" class="action-button"
      @click="${this.onMsbbSettingsClick_}">
      $i18n{freMsbbSettingsButton}
    </cr-button>
  </div>
</div>

<div id="appDialog" class="dialog" ?hidden="${!this.showMainAppDialog_}">
  <div id="heading">
    <h1>$i18n{dialogTitle}</h1>
    <cr-icon-button id="closeButton" class="close-button" iron-icon="cr:close"
        @click="${this.onClose_}" aria-label="$i18n{close}">
    </cr-icon-button>
  </div>

  <div id="bodyAndFooter">
    <div id="body">
      <compose-textarea id="textarea" .value="${this.input_}"
          @value-changed="${this.onInputValueChanged_}"
          ?readonly="${this.submitted_}"
          ?allow-exiting-readonly-mode="${!this.loading_}"
          @edit-click="${this.onEditClick_}"
          .inputParams="${this.inputParams_}">
      </compose-textarea>
      <div id="inputModesContainer" ?hidden="${!this.showInputModes_}">
        <cr-chip id="polishChip" selected
            ?selected="${this.isInputModeSelected_(InputMode.kPolish)}"
            @click="${this.onPolishChipClick_}">
          <cr-icon icon="${this.getChipIcon_(InputMode.kPolish)}"></cr-icon>
          $i18n{inputModeChipPolish}
        </cr-chip>
        <cr-chip id="elaborateChip"
            ?selected="${this.isInputModeSelected_(InputMode.kElaborate)}"
            @click="${this.onElaborateChipClick_}">
          <cr-icon icon="${this.getChipIcon_(InputMode.kElaborate)}"></cr-icon>
          $i18n{inputModeChipElaborate}
        </cr-chip>
        <cr-chip id="formalizeChip"
            ?selected="${this.isInputModeSelected_(InputMode.kFormalize)}"
            @click="${this.onFormalizeChipClick_}">
          <cr-icon icon="${this.getChipIcon_(InputMode.kFormalize)}"></cr-icon>
          $i18n{inputModeChipFormalize}
        </cr-chip>
      </div>
      <div id="loading" ?hidden="${!this.loadingIndicatorShown_}">
        <cr-loading-gradient>
          <svg xmlns="http://www.w3.org/2000/svg" width="100%" height="51">
            <clipPath>
              <rect x="0" y="0" width="100%" height="11" rx="4"></rect>
              <rect x="0" y="20" width="100%" height="10.8333" rx="4"></rect>
              <rect x="0" y="40" width="75%" height="11" rx="4"></rect>
            </clipPath>
          </svg>
        </cr-loading-gradient>
      </div>
      <div id="resultContainer" class="result-container"
          ?hidden="${this.hideResults_()}">
        <div id="resultContainerInner">
          <div id="resultTextContainer" class="cr-scrollable">
            <div class="cr-scrollable-top"></div>
            <compose-result-text id="resultText"
                .textInput="${this.responseText_}"
                ?is-output-complete="${this.outputComplete_}"
                @is-output-complete-changed="${this.onOutputCompleteChanged_}"
                ?has-output="${this.hasOutput_}"
                @has-output-changed="${this.onHasOutputChanged_}"
                ?has-partial-output="${this.hasPartialOutput_}"
                @has-partial-output-changed="${this.onHasPartialOutputChanged_}"
                @result-edit="${this.onResultEdit_}"
                @set-result-focus="${this.onSetResultFocus_}">
            </compose-result-text>
          </div>
          <div id="resultOptionsContainer">
            <div id="resultOptions" ?inert="${this.hideResults_()}">
              <select class="md-select" id="modifierMenu"
                  .value="${this.selectedModifier_}"
                  aria-label="$i18n{modifierMenuLabel}"
                  @change="${this.onModifierChanged_}"
                  @keydown="${this.openModifierMenuOnKeyDown_}">
                ${this.modifierOptions_.map(item => html`
                  <option value="${item.value}" ?disabled="${item.isDefault}"
                      ?selected="${item.isDefault}">
                    ${item.label}
                  </option>
                `)}
              </select>
              <div class="icon-buttons-row">
                <div class="button-container">
                  <cr-button id="undoButton" title="$i18n{undo}"
                      ?disabled="${!this.undoEnabled_}"
                      @click="${this.onUndoClick_}">
                    <div aria-hidden="true"> $i18n{undoButtonText} </div>
                    <cr-icon aria-hidden="true" slot="suffix-icon"
                      icon="compose:undo">
                    </cr-icon>
                  </cr-button>
                </div>
                <div class="button-container">
                  <cr-button id="redoButton" title="$i18n{redo}"
                      ?disabled="${!this.redoEnabled_}"
                      @click="${this.onRedoClick_}">
                    <div aria-hidden="true"> $i18n{redoButtonText} </div>
                    <cr-icon aria-hidden="true" slot="suffix-icon"
                      icon="compose:redo">
                    </cr-icon>
                  </cr-button>
                </div>
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>

    <div id="submitFooter" class="footer" ?hidden="${this.submitted_}">
      <div class="footer-text">
        <div @click="${this.onFooterClick_}" ?hidden="${this.enterprise_}">
          $i18nRaw{inputFooter}
        </div>
        <div @click="${this.onFooterClick_}" ?hidden="${!this.enterprise_}">
          $i18nRaw{inputFooterEnterprise}
        </div>
      </div>
      <cr-button id="submitButton" class="action-button"
          @click="${this.onSubmit_}"
          ?disabled="${!this.isSubmitEnabled_}">
        <cr-icon slot="prefix-icon" icon="compose:compose"></cr-icon>
        $i18n{submitButton}
      </cr-button>
    </div>

    <div id="resultFooter" class="footer"
        ?hidden="${this.hideResults_()}"
        ?inert="${this.hasPartialOutput_}">
      <div class="footer-text">
        <div @click="${this.onFooterClick_}">
          <b id="onDeviceUsedFooter"
              ?hidden="${!this.showOnDeviceDogfoodFooter_()}">
            $i18nRaw{onDeviceUsedFooter}
          </b>
          <span ?hidden="${!this.showOnDeviceDogfoodFooter_()}"
              >$i18nRaw{dogfoodFooter}</span>
          <span ?hidden="${this.showOnDeviceDogfoodFooter_()}"
              >$i18nRaw{resultFooter}</span>
        </div>
        <cr-feedback-buttons id="feedbackButtons"
            @selected-option-changed="${this.onFeedbackSelectedOptionChanged_}"
            .selectedOption="${this.feedbackState_}"
            ?disabled="${!this.feedbackEnabled_}">
        </cr-feedback-buttons>
      </div>
      <cr-button id="acceptButton" class="action-button"
          @click="${this.onAccept_}">
        ${this.acceptButtonText_()}
      </cr-button>
    </div>

    <div id="errorFooter" class="footer"
        ?hidden="${!this.hasFailedResponse_()}">
      <div @click="${this.onFooterClick_}" class="footer-text">
        <span ?hidden="${this.hasErrorWithLink_()}"
          >${this.failedResponseErrorText_()}</span>
        <span ?hidden="${!this.hasUnsupportedLanguageResponse_()}"
          >$i18nRaw{errorUnsupportedLanguage}</span>
        <span ?hidden="${!this.hasPermissionDeniedResponse_()}"
          >$i18nRaw{errorPermissionDenied}</span>
      </div>
      <cr-button id="errorGoBackButton" class="action-button"
          @click="${this.onErrorGoBackButton_}"
          ?hidden="${!this.isBackFromErrorAvailable_()}">
        <cr-icon aria-hidden="true" slot="prefix-icon" icon="compose:undo">
        </cr-icon>
        <div aria-hidden="true">
          $i18nRaw{errorFilteredGoBackButton}
        </div>
      </cr-button>
    </div>

    <div id="editContainer" ?hidden="${!this.isEditingSubmittedInput_}">
      <compose-textarea id="editTextarea" .value="${this.editedInput_}"
          @value-changed="${this.onEditedInputValueChanged_}"
          .inputParams="${this.inputParams_}">
      </compose-textarea>
      <div class="footer">
        <if expr="not(is_win)">
          <cr-button id="cancelEditButton" class="tonal-button"
              @click="${this.onCancelEditClick_}">
            $i18n{editCancelButton}
          </cr-button>
        </if>
        <cr-button id="submitEditButton" class="action-button"
            @click="${this.onSubmitEdit_}"
            ?disabled="${!this.isEditSubmitEnabled_}">
          $i18n{editUpdateButton}
        </cr-button>
        <if expr="is_win">
          <cr-button id="cancelEditButton" class="tonal-button"
              @click="${this.onCancelEditClick_}">
            $i18n{editCancelButton}
          </cr-button>
        </if>
      </div>
    </div>
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
