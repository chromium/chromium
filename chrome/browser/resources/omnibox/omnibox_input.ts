// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageClassification} from './omnibox.mojom-webui.js';
import {OmniboxElement} from './omnibox_element.js';
/* eslint-disable-next-line @typescript-eslint/ban-ts-comment */
// @ts-ignore:next-line
import sheet from './omnibox_input.css' with {type : 'css'};

export interface QueryInputs {
  inputText: string;
  resetAutocompleteController: boolean;
  cursorLock: boolean;
  cursorPosition: number;
  zeroSuggest: boolean;
  preventInlineAutocomplete: boolean;
  preferKeyword: boolean;
  currentUrl: string;
  pageClassification: number;
}

export interface DisplayInputs {
  showIncompleteResults: boolean;
  showDetails: boolean;
  showAllProviders: boolean;
  elideCells: boolean;
  thinRows: boolean;
}

export class OmniboxInput extends OmniboxElement {
  private elements: {
    top: HTMLElement,
    arrowPadding: HTMLElement,
    connectWindowOmnibox: HTMLInputElement,
    currentUrl: HTMLInputElement,
    elideCells: HTMLInputElement,
    exportClipboard: HTMLElement,
    exportFile: HTMLElement,
    filterText: HTMLInputElement,
    historyWarning: HTMLElement,
    importClipboard: HTMLElement,
    importedWarning: HTMLElement,
    importFile: HTMLElement,
    importFileInput: HTMLInputElement,
    inputText: HTMLInputElement,
    lockCursorPosition: HTMLInputElement,
    pageClassification: HTMLSelectElement,
    preferKeyword: HTMLInputElement,
    preventInlineAutocomplete: HTMLInputElement,
    processBatch: HTMLElement,
    processBatchInput: HTMLInputElement,
    resetAutocompleteController: HTMLInputElement,
    responsesCount: HTMLElement,
    responseSelection: HTMLInputElement,
    showAllProviders: HTMLInputElement,
    showDetails: HTMLInputElement,
    showIncompleteResults: HTMLInputElement,
    thinRows: HTMLInputElement,
    zeroSuggest: HTMLInputElement,
  };

  constructor() {
    super('omnibox-input-template');
    this.shadowRoot!.adoptedStyleSheets = [sheet];
  }

  connectedCallback() {
    this.elements = {
      top: this.$<HTMLElement>('#top')!,
      arrowPadding: this.$<HTMLElement>('#arrow-padding')!,
      connectWindowOmnibox: this.$<HTMLInputElement>('#connect-window-omnibox')!
      ,
      currentUrl: this.$<HTMLInputElement>('#current-url')!,
      elideCells: this.$<HTMLInputElement>('#elide-cells')!,
      exportClipboard: this.$<HTMLElement>('#export-clipboard')!,
      exportFile: this.$<HTMLElement>('#export-file')!,
      filterText: this.$<HTMLInputElement>('#filter-text')!,
      historyWarning: this.$<HTMLElement>('#history-warning')!,
      importClipboard: this.$<HTMLElement>('#import-clipboard')!,
      importedWarning: this.$<HTMLElement>('#imported-warning')!,
      importFile: this.$<HTMLElement>('#import-file')!,
      importFileInput: this.$<HTMLInputElement>('#import-file-input')!,
      inputText: this.$<HTMLInputElement>('#input-text')!,
      lockCursorPosition: this.$<HTMLInputElement>('#lock-cursor-position')!,
      pageClassification: this.$<HTMLSelectElement>('#page-classification')!,
      preferKeyword: this.$<HTMLInputElement>('#prefer-keyword')!,
      preventInlineAutocomplete:
          this.$<HTMLInputElement>('#prevent-inline-autocomplete')!,
      processBatch: this.$<HTMLElement>('#process-batch')!,
      processBatchInput: this.$<HTMLInputElement>('#process-batch-input')!,
      resetAutocompleteController:
          this.$<HTMLInputElement>('#reset-autocomplete-controller')!,
      responsesCount: this.$<HTMLElement>('#responses-count')!,
      responseSelection: this.$<HTMLInputElement>('#response-selection')!,
      showAllProviders: this.$<HTMLInputElement>('#show-all-providers')!,
      showDetails: this.$<HTMLInputElement>('#show-details')!,
      showIncompleteResults:
          this.$<HTMLInputElement>('#show-incomplete-results')!,
      thinRows: this.$<HTMLInputElement>('#thin-rows')!,
      zeroSuggest: this.$<HTMLInputElement>('#zero-suggest')!,
    };
    this.restoreInputs();
    this.setupElementListeners();
    this.addPageClassification();
  }

  // Add Page Classification labels as options to dropdown.
  private addPageClassification() {
    const dropdown = this.$<HTMLSelectElement>('#page-classification')!;
    for (const page in Object.keys(PageClassification)) {
      const label = PageClassification[page];
      // Filter out built-in reverse mappings for this numeric enum.
      if (label === undefined) {
        continue;
      }
      const option = document.createElement('option');
      option.value = page;
      option.text = label;
      // Pre-select the OTHER option.
      page === '4' ? option.selected = true : null;
      dropdown.appendChild(option);
    }
  }

  private storeInputs() {
    const inputs = {
      connectWindowOmnibox: this.connectWindowOmnibox,
      displayInputs: this.displayInputs,
    };
    window.localStorage.setItem('preserved-inputs', JSON.stringify(inputs));
  }

  private restoreInputs() {
    const inputsString = window.localStorage.getItem('preserved-inputs');
    const inputs = inputsString && JSON.parse(inputsString) || {};
    this.elements.connectWindowOmnibox.checked = inputs.connectWindowOmnibox;
    this.displayInputs =
        inputs.displayInputs || OmniboxInput.defaultDisplayInputs;
  }

  private setupElementListeners() {
    [this.elements.inputText,
     this.elements.resetAutocompleteController,
     this.elements.lockCursorPosition,
     this.elements.zeroSuggest,
     this.elements.preventInlineAutocomplete,
     this.elements.preferKeyword,
     this.elements.currentUrl,
     this.elements.pageClassification,
    ].forEach(element => {
      element.addEventListener('input', this.onQueryInputsChanged.bind(this));
    });

    // Set text of #arrow-padding to substring of #input-text text, from
    // beginning until cursor position, in order to correctly align .arrow-up.
    this.elements.inputText.addEventListener(
        'input', this.positionCursorPositionIndicators.bind(this));

    this.elements.connectWindowOmnibox.addEventListener(
        'input', this.storeInputs.bind(this));

    this.elements.responseSelection.addEventListener(
        'input', this.onResponseSelectionChanged.bind(this));
    this.elements.responseSelection.addEventListener(
        'blur', this.onResponseSelectionBlur.bind(this));

    [this.elements.showIncompleteResults,
     this.elements.showDetails,
     this.elements.showAllProviders,
     this.elements.elideCells,
     this.elements.thinRows,
    ].forEach(element => {
      element.addEventListener('input', this.onDisplayInputsChanged.bind(this));
    });

    this.elements.filterText.addEventListener(
        'input', this.onFilterInputsChanged.bind(this));

    this.elements.exportClipboard.addEventListener(
        'click', this.onExportClipboard.bind(this));
    this.elements.exportFile.addEventListener(
        'click', this.onExportFile.bind(this));
    this.elements.importClipboard.addEventListener(
        'click', this.onImportClipboard.bind(this));
    this.elements.importFileInput.addEventListener(
        'input', this.onImportFile.bind(this));
    this.elements.processBatchInput.addEventListener(
        'input', this.onProcessBatchFile.bind(this));

    this.setupDragListeners(this.elements.top);
    this.elements.top.addEventListener('drop', this.onImportDropped.bind(this));

    this.setupDragListeners(this.elements.processBatch);
    this.elements.processBatch.addEventListener(
        'drop', this.onProcessBatchDropped.bind(this));

    this.$all<HTMLElement>('.button').forEach(
        el => el.addEventListener('keypress', (e: KeyboardEvent) => {
          if (e.key === ' ' || e.key === 'Enter') {
            el.click();
          }
        }));
  }

  /**
   * Sets up boilerplate event listeners for an element that is able to receive
   * drag events.
   */
  private setupDragListeners(element: Element) {
    // There are 2 classes toggled during drags:
    // - `drag-background` alters the `element`'s background when the mouse is
    //   over it to indicate it's a drag target.
    // - `drag-silence` silences mouse events from the children of `element`
    //   while the drag is active. This is necessary to avoid receiving
    //   `dragenter` & `dragleave` events when children of `element` are entered
    //   or left.
    // Ideally, there'd be just 1 class controlling both behaviors and active
    // when dragging over `element`. However, `dragenter` and `dragleave` events
    // are racy (see https://stackoverflow.com/questions/7110353). So instead,
    // toggle `drag-background` when the dragging-mouse enters/leaves `element`.
    // And toggle `drag-silence` when the mouse begins/stops dragging. This
    // workaround isn't 100% accurate, but all the other workarounds (e.g. those
    // listed in the stackoverflow answers), were significantly worse.
    element.addEventListener('dragenter', () => {
      element.classList.add('drag-background');
      element.classList.add('drag-silence');
    });
    element.addEventListener(
        'dragleave', () => element.classList.remove('drag-background'));
    element.addEventListener('dragover', e => e.preventDefault());
    element.addEventListener('drop', e => {
      e.preventDefault();
      element.classList.remove('drag-background');
      element.classList.remove('drag-silence');
    });
    element.addEventListener(
        'mouseenter', () => element.classList.remove('drag-silence'));
  }

  private onQueryInputsChanged() {
    this.elements.importedWarning.hidden = true;
    this.dispatchEvent(
        new CustomEvent('query-inputs-changed', {detail: this.queryInputs}));
  }

  get queryInputs(): QueryInputs {
    return {
      inputText: this.elements.inputText.value,
      resetAutocompleteController:
          this.elements.resetAutocompleteController.checked,
      cursorLock: this.elements.lockCursorPosition.checked,
      cursorPosition: this.cursorPosition,
      zeroSuggest: this.elements.zeroSuggest.checked,
      preventInlineAutocomplete:
          this.elements.preventInlineAutocomplete.checked,
      preferKeyword: this.elements.preferKeyword.checked,
      currentUrl: this.elements.currentUrl.value,
      pageClassification: Number(this.elements.pageClassification.value),
    };
  }

  set queryInputs(queryInputs: QueryInputs) {
    this.elements.inputText.value = queryInputs.inputText;
    this.elements.resetAutocompleteController.checked =
        queryInputs.resetAutocompleteController;
    this.elements.lockCursorPosition.checked = queryInputs.cursorLock;
    this.cursorPosition = queryInputs.cursorPosition;
    this.elements.zeroSuggest.checked = queryInputs.zeroSuggest;
    this.elements.preventInlineAutocomplete.checked =
        queryInputs.preventInlineAutocomplete;
    this.elements.preferKeyword.checked = queryInputs.preferKeyword;
    this.elements.currentUrl.value = queryInputs.currentUrl;
    this.elements.pageClassification.value =
        String(queryInputs.pageClassification);
  }

  private get cursorPosition(): number {
    return this.elements.lockCursorPosition.checked ?
        this.elements.inputText.value.length :
        Number(this.elements.inputText.selectionEnd);
  }

  private set cursorPosition(value: number) {
    this.elements.inputText.setSelectionRange(value, value);
    this.positionCursorPositionIndicators();
  }

  private positionCursorPositionIndicators() {
    this.elements.arrowPadding.textContent =
        this.elements.inputText.value.substring(0, this.cursorPosition);
  }

  get connectWindowOmnibox(): boolean {
    return this.elements.connectWindowOmnibox.checked;
  }

  private onResponseSelectionChanged() {
    const {value, max} = this.elements.responseSelection;
    this.elements.historyWarning.hidden = value === '0' || value === max;
    this.dispatchEvent(
        new CustomEvent('response-select', {detail: Number(value) - 1}));
  }

  private onResponseSelectionBlur() {
    const {value, min, max} = this.elements.responseSelection;
    this.elements.responseSelection.value =
        String(Math.max(Math.min(Number(value), Number(max)), Number(min)));
    this.onResponseSelectionChanged();
  }

  set responsesCount(value: number) {
    if (this.elements.responseSelection.value ===
        this.elements.responseSelection.max) {
      this.elements.responseSelection.value = String(value);
    }
    this.elements.responseSelection.max = String(value);
    this.elements.responseSelection.min = String(value ? 1 : 0);
    this.elements.responsesCount.textContent = String(value);
    this.onResponseSelectionBlur();
  }

  private onDisplayInputsChanged() {
    this.storeInputs();
    this.dispatchEvent(new CustomEvent(
        'display-inputs-changed', {detail: this.displayInputs}));
  }

  get displayInputs(): DisplayInputs {
    return {
      showIncompleteResults: this.elements.showIncompleteResults.checked,
      showDetails: this.elements.showDetails.checked,
      showAllProviders: this.elements.showAllProviders.checked,
      elideCells: this.elements.elideCells.checked,
      thinRows: this.elements.thinRows.checked,
    };
  }

  set displayInputs(displayInputs: DisplayInputs) {
    this.elements.showIncompleteResults.checked =
        displayInputs.showIncompleteResults;
    this.elements.showDetails.checked = displayInputs.showDetails;
    this.elements.showAllProviders.checked = displayInputs.showAllProviders;
    this.elements.elideCells.checked = displayInputs.elideCells;
    this.elements.thinRows.checked = displayInputs.thinRows;
  }

  private onFilterInputsChanged() {
    this.dispatchEvent(new CustomEvent(
        'filter-input-changed', {detail: this.elements.filterText.value}));
  }

  private onExportClipboard() {
    this.dispatchEvent(new CustomEvent('export-clipboard'));
  }

  private onExportFile() {
    this.dispatchEvent(new CustomEvent('export-file'));
  }

  private async onImportClipboard() {
    this.import(await navigator.clipboard.readText());
  }

  private onImportFile(event: Event) {
    const file = (event.target as HTMLInputElement).files?.[0];
    if (file) {
      this.importFile(file);
    }
  }

  private onProcessBatchFile(event: Event) {
    const file = (event.target as HTMLInputElement).files?.[0];
    if (file) {
      this.processBatchFile(file);
    }
  }

  private onImportDropped(event: DragEvent) {
    const data = event.dataTransfer!;
    const dragText = data.getData('Text');
    if (dragText) {
      this.import(dragText);
    } else if (data.files[0]) {
      this.importFile(data.files[0]);
    }
  }

  private onProcessBatchDropped(event: DragEvent) {
    const data = event.dataTransfer!;
    const dragText = data.getData('Text');
    if (dragText) {
      this.processBatch(dragText);
    } else if (data.files[0]) {
      this.processBatchFile(data.files[0]);
    }
  }

  private importFile(file: File) {
    OmniboxInput.readFile(file).then(this.import.bind(this));
  }

  private processBatchFile(file: File) {
    OmniboxInput.readFile(file).then(this.processBatch.bind(this));
  }

  private import(importString: string) {
    try {
      const importData = JSON.parse(importString);
      // TODO(manukh): If import fails, this UI state change shouldn't happen.
      this.elements.importedWarning.hidden = false;
      this.dispatchEvent(new CustomEvent('import', {detail: importData}));
    } catch (error) {
      console.error('error during import, invalid json:', error);
    }
  }

  private processBatch(processBatchString: string) {
    try {
      const processBatchData = JSON.parse(processBatchString);
      this.dispatchEvent(
          new CustomEvent('process-batch', {detail: processBatchData}));
    } catch (error) {
      console.error('error during process batch, invalid json:', error);
    }
  }

  private static readFile(file: File): Promise<string> {
    return new Promise(resolve => {
      const reader = new FileReader();
      reader.onloadend = () => {
        if (reader.readyState === FileReader.DONE) {
          resolve(reader.result as string);
        } else {
          console.error('error importing, unable to read file:', reader.error);
        }
      };
      reader.readAsText(file);
    });
  }

  static get defaultDisplayInputs(): DisplayInputs {
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
