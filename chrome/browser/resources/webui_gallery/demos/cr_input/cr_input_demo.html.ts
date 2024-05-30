// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CrInputDemoElement} from './cr_input_demo.js';

export function getHtml(this: CrInputDemoElement) {
  return html`
<h1>cr-input</h1>
<h2>Default "filled" inputs with built-in labels</h2>
<div class="demos">
  <cr-input
      class="no-error"
      type="text"
      label="Standard input"
      placeholder="Placeholder text"
      .value="${this.textValue_}"
      @value-changed="${this.onTextValueChanged_}">
  </cr-input>

  <cr-input
      class="no-error"
      type="search"
      placeholder="Search a query"
      .value="${this.searchValue_}"
      @value-changed="${this.onSearchValueChanged_}">
    <div slot="inline-prefix" class="cr-icon icon-search" alt=""></div>
    <cr-icon-button class="icon-cancel"
        ?hidden="${!this.searchValue_}" slot="inline-suffix"
        @click="${this.onClearSearchClick_}"
        title="Clear search">
    </cr-icon-button>
  </cr-input>

  <cr-input
      class="no-error"
      label="Email address"
      type="text"
      placeholder="username"
      .value="${this.emailValue_}"
      @value-changed="${this.onEmailValueChanged_}">
    <div slot="inline-suffix" class="domain-name">@chromium.org</div>
  </cr-input>

  <cr-input
      id="numberInput"
      label="Number input"
      type="number"
      min="5"
      max="200"
      placeholder="A number between 5 and 200"
      error-message="Number needs to be between 5 and 200"
      .value="${this.numberValue_}"
      @value-changed="${this.onNumberValueChanged_}">
    <cr-button slot="suffix" @click="${this.onValidateClick_}">
      Validate
    </cr-button>
  </cr-input>

  <cr-input
      label="Auto-validating pin"
      type="password"
      placeholder="Enter a pin of 4 digits"
      pattern="[0-9]{4}"
      error-message="Pin must be 4 digits"
      .value="${this.pinValue_}" @value-changed="${this.onPinValueChanged_}"
      auto-validate>
  </cr-input>

  <cr-input
      class="no-error"
      type="text"
      label="Disabled input"
      disabled
      value="The value cannot be changed">
  </cr-input>

  <cr-input
      class="no-error"
      type="text"
      label="Readonly input"
      readonly
      value="The value cannot be changed">
  </cr-input>

  <cr-textarea
      type="text"
      label="Textarea"
      .value="${this.textareaValue_}"
      @value-changed="${this.onTextareaValueChanged_}">
  </cr-textarea>

  <div>
    <div>Text input value: ${this.textValue_}</div>
    <div>Search input value: ${this.searchValue_}</div>
    <div>Email input value: ${this.emailValue_}</div>
    <div>Number input value: ${this.numberValue_}</div>
    <div>Pin input value: ${this.pinValue_}</div>
    <div>Textarea value: ${this.textareaValue_}</div>
  </div>
</div>

<h2>"Stroked" inputs without built-in labels</h2>
<div class="demos">
  <cr-input class="stroked no-error"
      type="text" placeholder="Insert text here" aria-label="Some label">
  </cr-input>

  <cr-input required auto-validate invalid class="stroked"
      type="text" placeholder="Required field" aria-label="Some label">
  </cr-input>

  <div class="row center">
    <label aria-hidden="true">Some visible external label</label>
    <cr-input class="stroked no-error"
        type="text" placeholder="Insert text here" aria-label="Some label">
    </cr-input>
  </div>
</div>`;
}
