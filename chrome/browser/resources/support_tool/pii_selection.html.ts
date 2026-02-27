// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {PiiSelectionElement} from './pii_selection.js';
import {PiiRadioButtons} from './pii_selection.js';

export function getHtml(this: PiiSelectionElement) {
  // clang-format off
  return html`<!--html_template_start_-->
<h1 tabindex="0">${this.i18n('reviewPiiPageTitle')}</h1>
<div id="pii-warning-text" class="support-tool-title" tabindex="0">
  ${this.i18n('piiWarningText')}
</div>
<div id="radio-group">
  <cr-radio-group .selected="${this.selectedRadioButton_}"
      @selected-changed="${this.onSelectedRadioButtonSelectedChanged_}">
    <cr-radio-button name="${PiiRadioButtons.INCLUDE_ALL}" tabindex="0">
      ${this.i18n('includeAllPiiRadioButton')}
    </cr-radio-button>
    <cr-radio-button name="${PiiRadioButtons.INCLUDE_NONE}" tabindex="0">
      ${this.i18n('removePiiRadioButton')}
    </cr-radio-button>
    <cr-collapse id="privacy-disclaimer" ?opened="${this.showDisclaimer_()}">
      <div tabindex="0">${this.i18n('piiRemovalDisclaimer')}</div>
    </cr-collapse>
    <cr-radio-button name="${PiiRadioButtons.INCLUDE_SOME}" tabindex="0">
      ${this.i18n('manuallySelectPiiRadioButton')}
    </cr-radio-button>
  </cr-radio-group>
</div>
<cr-collapse id="detected-pii-container" ?opened="${this.showPIISelection_}">
  <div>
    ${this.detectedPiiItems_.map((item, index) => html`
      <div class="detected-pii-item">
        <cr-expand-button ?expanded="${item.expandDetails}"
            data-index="${index}"
            @expanded-changed="${this.onPiiExpandedChanged_}"
            aria-label="${this.getPiiItemAriaLabel_(item)}">
          <cr-checkbox ?checked="${item.keep}" data-index="${index}"
              @checked-changed="${this.onPiiCheckedChanged_}">
            <span>${item.piiTypeDescription}: ${item.count}</span>
          </cr-checkbox>
        </cr-expand-button>
        <cr-collapse class="pii-item-collapse" ?opened="${item.expandDetails}">
          <div class="pii-details" tabindex="0">
            ${item.detectedData.map(item => html`<div>${item}</div>`)}
          </div>
        </cr-collapse>
      </div>
    `)}
  </div>
</cr-collapse>
<div class="navigation-buttons">
  <cr-button id="cancelButton" @click="${this.onCancelClick_}">
    ${this.i18n('cancelButtonText')}
  </cr-button>
  <cr-button id="exportButton" class="action-button"
      @click="${this.onExportClick_}">
    ${this.i18n('exportButtonText')}
  </cr-button>
</div>
  <!--html_template_end_-->`;
  // clang-format on
}
