// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {OmniboxElement} from './omnibox_element.js';

/**
 * @typedef {{
 *   inputText: string,
 *   resetAutocompleteController: boolean,
 *   cursorLock: boolean,
 *   cursorPosition: number,
 *   zeroSuggest: boolean,
 *   preventInlineAutocomplete: boolean,
 *   preferKeyword: boolean,
 *   currentUrl: string,
 *   pageClassification: number,
 * }}
 */
export let QueryInputs;

/**
 * @typedef {{
 *   showIncompleteResults: boolean,
 *   showDetails: boolean,
 *   showAllProviders: boolean,
 *   elideCells: boolean,
 *   thinRows: boolean,
 * }}
 */
export let DisplayInputs;

export class OmniboxInput extends OmniboxElement {
  constructor() {
    super('omnibox-input-template');
    this.restoreInputs_();
  }

  /** @override */
  connectedCallback() {
    this.setupElementListeners_();
  }

  /** @private */
  storeInputs_() {
    const inputs = {
      connectWindowOmnibox: this.connectWindowOmnibox,
      displayInputs: this.displayInputs,
    };
    window.localStorage.setItem('preserved-inputs', JSON.stringify(inputs));
  }

  /** @private */
  restoreInputs_() {
    const inputsString = window.localStorage.getItem('preserved-inputs');
    const inputs = inputsString && JSON.parse(inputsString) || {};
    this.$('#connect-window-omnibox').checked = inputs.connectWindowOmnibox;
    this.displayInputs =
        inputs.displayInputs || OmniboxInput.defaultDisplayInputs;
  }

  /** @private */
  setupElementListeners_() {
    ['#input-text',
     '#reset-autocomplete-controller',
     '#lock-cursor-position',
     '#zero-suggest',
     '#prevent-inline-autocomplete',
     '#prefer-keyword',
     '#current-url',
     '#page-classification',
    ].forEach(query => {
      this.$(query).addEventListener(
          'input', this.onQueryInputsChanged_.bind(this));
    });

    // Set text of .arrow-padding to substring of #input-text text, from
    // beginning until cursor position, in order to correctly align .arrow-up.
    this.$('#input-text')
        .addEventListener(
            'input', this.positionCursorPositionIndicators_.bind(this));

    this.$('#connect-window-omnibox')
        .addEventListener('input', this.storeInputs_.bind(this));

    this.$('#response-selection')
        .addEventListener('input', this.onResponseSelectionChanged_.bind(this));
    this.$('#response-selection')
        .addEventListener('blur', this.onResponseSelectionBlur_.bind(this));

    ['#show-incomplete-results',
     '#show-details',
     '#show-all-providers',
     '#elide-cells',
     '#thin-rows',
    ].forEach(query => {
      this.$(query).addEventListener(
          'input', this.onDisplayInputsChanged_.bind(this));
    });

    this.$('#filter-text')
        .addEventListener('input', this.onFilterInputsChanged_.bind(this));

    this.$('#export-clipboard')
        .addEventListener('click', this.onExportClipboard_.bind(this));
    this.$('#export-file')
        .addEventListener('click', this.onExportFile_.bind(this));
    this.$('#import-clipboard')
        .addEventListener('click', this.onImportClipboard_.bind(this));
    this.$('#import-file-input')
        .addEventListener('input', this.onImportFile_.bind(this));
    this.$('#process-batch-input')
        .addEventListener('input', this.onProcessBatchFile_.bind(this));
    ['#import-clipboard', '#import-file'].forEach(query => {
      this.setupDragListeners_(this.$(query));
      this.$(query).addEventListener('drop', this.onImportDropped_.bind(this));
    });
    this.setupDragListeners_(this.$('#process-batch'));
    this.$('#process-batch')
        .addEventListener('drop', this.onProcessBatchDropped_.bind(this));

    this.$$('.button').forEach(el => el.addEventListener('keypress', e => {
      if (e.key === ' ' || e.key === 'Enter') {
        el.click();
      }
    }));
  }

  /**
   * Sets up boilerplate event listeners for an element that is able to receive
   * drag events.
   * @private @param {!Element} element
   */
  setupDragListeners_(element) {
    element.addEventListener(
        'dragenter', () => element.classList.add('drag-hover'));
    element.addEventListener(
        'dragleave', () => element.classList.remove('drag-hover'));
    element.addEventListener('dragover', e => e.preventDefault());
    element.addEventListener('drop', e => {
      e.preventDefault();
      element.classList.remove('drag-hover');
    });
  }

  /** @private */
  onQueryInputsChanged_() {
    this.$('#imported-warning').hidden = true;
    this.$('#current-url').disabled = this.$('#zero-suggest').checked;
    if (this.$('#zero-suggest').checked) {
      this.$('#current-url').value = this.$('#input-text').value;
    }
    this.dispatchEvent(
        new CustomEvent('query-inputs-changed', {detail: this.queryInputs}));
  }

  /** @return {QueryInputs} */
  get queryInputs() {
    return {
      inputText: this.$('#input-text').value,
      resetAutocompleteController:
          this.$('#reset-autocomplete-controller').checked,
      cursorLock: this.$('#lock-cursor-position').checked,
      cursorPosition: this.cursorPosition_,
      zeroSuggest: this.$('#zero-suggest').checked,
      preventInlineAutocomplete: this.$('#prevent-inline-autocomplete').checked,
      preferKeyword: this.$('#prefer-keyword').checked,
      currentUrl: this.$('#current-url').value,
      pageClassification: this.$('#page-classification').value,
    };
  }

  /** @param {QueryInputs} queryInputs */
  set queryInputs(queryInputs) {
    this.$('#input-text').value = queryInputs.inputText;
    this.$('#reset-autocomplete-controller').checked =
        queryInputs.resetAutocompleteController;
    this.$('#lock-cursor-position').checked = queryInputs.cursorLock;
    this.cursorPosition_ = queryInputs.cursorPosition;
    this.$('#zero-suggest').checked = queryInputs.zeroSuggest;
    this.$('#prevent-inline-autocomplete').checked =
        queryInputs.preventInlineAutocomplete;
    this.$('#prefer-keyword').checked = queryInputs.preferKeyword;
    this.$('#current-url').value = queryInputs.currentUrl;
    this.$('#page-classification').value = queryInputs.pageClassification;
  }

  /** @private @return {number} */
  get cursorPosition_() {
    return this.$('#lock-cursor-position').checked ?
        this.$('#input-text').value.length :
        this.$('#input-text').selectionEnd;
  }

  /** @private @param {number} value */
  set cursorPosition_(value) {
    this.$('#input-text').setSelectionRange(value, value);
    this.positionCursorPositionIndicators_();
  }

  /** @private */
  positionCursorPositionIndicators_() {
    this.$('.arrow-padding').textContent =
        this.$('#input-text').value.substring(0, this.cursorPosition_);
  }

  /** @return {boolean} */
  get connectWindowOmnibox() {
    return this.$('#connect-window-omnibox').checked;
  }

  /** @private @param {boolean} connectWindowOmnibox */
  set connectWindowOmnibox_(connectWindowOmnibox) {
    this.$('#connect-window-omnibox').checked = connectWindowOmnibox;
  }

  /** @private */
  onResponseSelectionChanged_() {
    const {value, max} = this.$('#response-selection');
    this.$('#history-warning').hidden = value === '0' || value === max;
    this.dispatchEvent(new CustomEvent('response-select', {detail: value - 1}));
  }

  /** @private */
  onResponseSelectionBlur_() {
    const {value, min, max} = this.$('#response-selection');
    this.$('#response-selection').value = Math.max(Math.min(value, max), min);
    this.onResponseSelectionChanged_();
  }

  /** @param {number} value */
  set responsesCount(value) {
    if (this.$('#response-selection').value ===
        this.$('#response-selection').max) {
      this.$('#response-selection').value = value;
    }
    this.$('#response-selection').max = value;
    this.$('#response-selection').min = value ? 1 : 0;
    this.$('#responses-count').textContent = value;
    this.onResponseSelectionBlur_();
  }

  /** @private */
  onDisplayInputsChanged_() {
    this.storeInputs_();
    this.dispatchEvent(new CustomEvent(
        'display-inputs-changed', {detail: this.displayInputs}));
  }

  /** @return {DisplayInputs} */
  get displayInputs() {
    return {
      showIncompleteResults: this.$('#show-incomplete-results').checked,
      showDetails: this.$('#show-details').checked,
      showAllProviders: this.$('#show-all-providers').checked,
      elideCells: this.$('#elide-cells').checked,
      thinRows: this.$('#thin-rows').checked,
    };
  }

  /** @param {DisplayInputs} displayInputs */
  set displayInputs(displayInputs) {
    this.$('#show-incomplete-results').checked =
        displayInputs.showIncompleteResults;
    this.$('#show-details').checked = displayInputs.showDetails;
    this.$('#show-all-providers').checked = displayInputs.showAllProviders;
    this.$('#elide-cells').checked = displayInputs.elideCells;
    this.$('#thin-rows').checked = displayInputs.thinRows;
  }

  /** @private */
  onFilterInputsChanged_() {
    this.dispatchEvent(new CustomEvent(
        'filter-input-changed', {detail: this.$('#filter-text').value}));
  }

  /** @private */
  onExportClipboard_() {
    this.dispatchEvent(new CustomEvent('export-clipboard'));
  }

  /** @private */
  onExportFile_() {
    this.dispatchEvent(new CustomEvent('export-file'));
  }

  /** @private */
  async onImportClipboard_() {
    this.import_(await navigator.clipboard.readText());
  }

  /** @private @param {!Event} event */
  onImportFile_(event) {
    this.importFile_(event.target.files[0]);
  }

  /** @private @param {!Event} event */
  onProcessBatchFile_(event) {
    this.processBatchFile_(event.target.files[0]);
  }

  /** @private @param {!Event} event */
  onImportDropped_(event) {
    const dragText = event.dataTransfer.getData('Text');
    if (dragText) {
      this.import_(dragText);
    } else if (event.dataTransfer.files[0]) {
      this.importFile_(event.dataTransfer.files[0]);
    }
  }

  /** @private @param {!Event} event */
  onProcessBatchDropped_(event) {
    const dragText = event.dataTransfer.getData('Text');
    if (dragText) {
      this.processBatch_(dragText);
    } else if (event.dataTransfer.files[0]) {
      this.processBatchFile_(event.dataTransfer.files[0]);
    }
  }

  /** @private @param {!File} file */
  importFile_(file) {
    OmniboxInput.readFile_(file).then(this.import_.bind(this));
  }

  /** @private @param {!File} file */
  processBatchFile_(file) {
    OmniboxInput.readFile_(file).then(this.processBatch_.bind(this));
  }

  /** @private @param {string} importString */
  import_(importString) {
    try {
      const importData = JSON.parse(importString);
      // TODO(manukh): If import fails, this UI state change shouldn't happen.
      this.$('#imported-warning').hidden = false;
      this.dispatchEvent(new CustomEvent('import', {detail: importData}));
    } catch (error) {
      console.error('error during import, invalid json:', error);
    }
  }

  /** @private @param {string} processBatchString */
  processBatch_(processBatchString) {
    try {
      const processBatchData = JSON.parse(processBatchString);
      this.dispatchEvent(
          new CustomEvent('process-batch', {detail: processBatchData}));
    } catch (error) {
      console.error('error during process batch, invalid json:', error);
    }
  }

  /**
   * @private
   * @param {!File} file
   * @return {!Promise}
   */
  static readFile_(file) {
    return new Promise(resolve => {
      const reader = new FileReader();
      reader.onloadend = () => {
        if (reader.readyState === FileReader.DONE) {
          resolve(/** @type {string} */(reader.result));
        } else {
          console.error('error importing, unable to read file:', reader.error);
        }
      };
      reader.readAsText(file);
    });
  }

  /** @return {DisplayInputs} */
  static get defaultDisplayInputs() {
    return {
      showIncompleteResults: false,
      showDetails: false,
      showAllProviders: true,
      elideCells: true,
      thinRows: false,
    };
  }
}

customElements.define('omnibox-input', OmniboxInput);
