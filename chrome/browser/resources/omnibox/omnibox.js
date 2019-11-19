// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for omnibox.html, served from chrome://omnibox/
 * This is used to debug omnibox ranking.  The user enters some text
 * into a box, submits it, and then sees lots of debug information
 * from the autocompleter that shows what omnibox would do with that
 * input.
 *
 * The simple object defined in this javascript file listens for
 * certain events on omnibox.html, sends (when appropriate) the
 * input text to C++ code to start the omnibox autcomplete controller
 * working, and listens from callbacks from the C++ code saying that
 * results are available.  When results (possibly intermediate ones)
 * are available, the Javascript formats them and displays them.
 */

(function() {
/**
 * @typedef {{
 *   inputText: string,
 *   callback: function(!mojom.OmniboxResponse):Promise,
 *   display: boolean,
 * }}
 */
let Request;

/**
  * @typedef {{
  *   batchMode: string,
  *   batchQueryInputs: Array<QueryInputs>,
  * }}
  */
let BatchSpecifier;

/**
 * @typedef {{
 *   queryInputs: QueryInputs,
 *   displayInputs: DisplayInputs,
 *   responsesHistory: !Array<!Array<!mojom.OmniboxResponse>>,
 * }}
 */
let OmniboxExport;

/** @type {!BrowserProxy} */
let browserProxy;
/** @type {!OmniboxInput} */
let omniboxInput;
/** @type {!omnibox_output.OmniboxOutput} */
let omniboxOutput;
/** @type {!ExportDelegate} */
let exportDelegate;

class BrowserProxy {
  /** @param {!omnibox_output.OmniboxOutput} omniboxOutput */
  constructor(omniboxOutput) {
    /** @private {!mojom.OmniboxPageCallbackRouter} */
    this.callbackRouter_ = new mojom.OmniboxPageCallbackRouter;

    this.callbackRouter_.handleNewAutocompleteResponse.addListener(
        this.handleNewAutocompleteResponse.bind(this));
    this.callbackRouter_.handleNewAutocompleteQuery.addListener(
        this.handleNewAutocompleteQuery.bind(this));
    this.callbackRouter_.handleAnswerImageData.addListener(
        omniboxOutput.updateAnswerImage.bind(omniboxOutput));

    /** @private {!mojom.OmniboxPageHandlerRemote} */
    this.handler_ = mojom.OmniboxPageHandler.getRemote();
    this.handler_.setClientPage(
        this.callbackRouter_.$.bindNewPipeAndPassRemote());

    /** @private {?Request} */
    this.lastRequest;
  }


  /**
   * @param {!mojom.OmniboxResponse} response
   * @param {boolean} isPageController
   */
  handleNewAutocompleteResponse(response, isPageController) {
    // Note: Using inputText is a sufficient fix for the way this is used today,
    // but in principle it would be better to associate requests with responses
    // using a unique session identifier, for example by rolling an integer each
    // time a request is made. Doing so would require extra bookkeeping on the
    // host side, so for now we keep it simple.
    const isForLastPageRequest = isPageController && this.lastRequest &&
        this.lastRequest.inputText === response.inputText;

    // When unfocusing the browser omnibox, the autocomplete controller
    // sends a response with no combined results. This response is ignored
    // in order to prevent the previous non-empty response from being
    // hidden and because these results wouldn't normally be displayed by
    // the browser window omnibox.
    if (isForLastPageRequest && this.lastRequest.display ||
        omniboxInput.connectWindowOmnibox && !isPageController &&
            response.combinedResults.length) {
      omniboxOutput.addAutocompleteResponse(response);
    }

    // TODO(orinj|manukh): If |response.done| but not |isForLastPageRequest|
    // then callback is being dropped. We should guarantee that callback is
    // always called because some callers await promises.
    if (isForLastPageRequest && response.done) {
      this.lastRequest.callback(response);
      this.lastRequest = null;
    }
  }

  /**
   * @param {boolean} isPageController
   * @param {string} inputText
   */
  handleNewAutocompleteQuery(isPageController, inputText) {
    // If the request originated from the debug page and is not for display,
    // then we don't want to clear the omniboxOutput.
    if (isPageController && this.lastRequest &&
            this.lastRequest.inputText === inputText &&
            this.lastRequest.display ||
        omniboxInput.connectWindowOmnibox && !isPageController) {
      omniboxOutput.prepareNewQuery();
    }
  }

  /**
   * @param {string} inputText
   * @param {boolean} resetAutocompleteController
   * @param {number} cursorPosition
   * @param {boolean} zeroSuggest
   * @param {boolean} preventInlineAutocomplete
   * @param {boolean} preferKeyword
   * @param {string} currentUrl
   * @param {number} pageClassification
   * @param {boolean} display
   * @return {!Promise}
   */
  makeRequest(
      inputText, resetAutocompleteController, cursorPosition, zeroSuggest,
      preventInlineAutocomplete, preferKeyword, currentUrl, pageClassification,
      display) {
    return new Promise(resolve => {
      this.lastRequest = {inputText, callback: resolve, display};
      this.handler_.startOmniboxQuery(
          inputText, resetAutocompleteController, cursorPosition, zeroSuggest,
          preventInlineAutocomplete, preferKeyword, currentUrl,
          pageClassification);
    });
  }
}

document.addEventListener('DOMContentLoaded', () => {
  omniboxInput = /** @type {!OmniboxInput} */ ($('omnibox-input'));
  omniboxOutput =
      /** @type {!omnibox_output.OmniboxOutput} */ ($('omnibox-output'));
  browserProxy = new BrowserProxy(omniboxOutput);
  exportDelegate = new ExportDelegate(omniboxOutput, omniboxInput);

  omniboxInput.addEventListener('query-inputs-changed', e => {
    browserProxy.makeRequest(
        e.detail.inputText, e.detail.resetAutocompleteController,
        e.detail.cursorPosition, e.detail.zeroSuggest,
        e.detail.preventInlineAutocomplete, e.detail.preferKeyword,
        e.detail.currentUrl, e.detail.pageClassification, true);
  });
  omniboxInput.addEventListener(
      'display-inputs-changed',
      e => omniboxOutput.updateDisplayInputs(e.detail));
  omniboxInput.addEventListener(
      'filter-input-changed', e => omniboxOutput.updateFilterText(e.detail));
  omniboxInput.addEventListener('import', e => exportDelegate.import(e.detail));
  omniboxInput.addEventListener(
      'process-batch', e => exportDelegate.processBatchData(e.detail));
  omniboxInput.addEventListener(
      'export-clipboard', () => exportDelegate.exportClipboard());
  omniboxInput.addEventListener(
      'export-file', () => exportDelegate.exportFile());
  omniboxInput.addEventListener(
      'response-select',
      e => omniboxOutput.updateSelectedResponseIndex(e.detail));

  omniboxOutput.addEventListener(
      'responses-count-changed', e => omniboxInput.responsesCount = e.detail);
});

class ExportDelegate {
  /**
   * @param {!omnibox_output.OmniboxOutput} omniboxOutput
   * @param {!OmniboxInput} omniboxInput
   */
  constructor(omniboxOutput, omniboxInput) {
    /** @private {!OmniboxInput} */
    this.omniboxInput_ = omniboxInput;
    /** @private {!omnibox_output.OmniboxOutput} */
    this.omniboxOutput_ = omniboxOutput;
  }

  /**
   * Import a single data item previously exported.
   * @param {OmniboxExport} importData
   * @return {boolean} true if a single data item was imported for viewing;
   * false if import failed.
   */
  import(importData) {
    if (!validateImportData_(importData)) {
      // TODO(manukh): Make use of this return value to fix the UI state
      // bug in omnibox_input.js -- see the related TODO there.
      return false;
    }
    this.omniboxInput_.queryInputs = importData.queryInputs;
    this.omniboxInput_.displayInputs = importData.displayInputs;
    this.omniboxOutput_.updateDisplayInputs(importData.displayInputs);
    this.omniboxOutput_.setResponsesHistory(importData.responsesHistory);
    return true;
  }

  /**
   * This is the worker function that transforms query inputs to accumulate
   * batch exports, then finally initiates a download for the complete set.
   * @param {!Array<!QueryInputs>} batchQueryInputs
   * @param {string} batchName
   */
  async processBatch(batchQueryInputs, batchName) {
    const batchExports = [];
    for (const queryInputs of batchQueryInputs) {
      const omniboxResponse = await browserProxy
        .makeRequest(
          queryInputs.inputText, queryInputs.resetAutocompleteController,
          queryInputs.cursorPosition, queryInputs.zeroSuggest,
          queryInputs.preventInlineAutocomplete, queryInputs.preferKeyword,
          queryInputs.currentUrl, queryInputs.pageClassification, false);
      const exportData = {
        queryInputs,
        // TODO(orinj|manukh): Make the schema consistent and remove
        // the extra level of array nesting.  [[This]] is done for now
        // so that elements can be extracted in the form import expects.
        responsesHistory: [[omniboxResponse]],
        displayInputs: this.omniboxInput_.displayInputs,
      };
      batchExports.push(exportData);
    }
    const variationInfo =
        await cr.sendWithPromise('requestVariationInfo', true);
    const pathInfo = await cr.sendWithPromise('requestPathInfo');
    const loadTimeDataKeys = ['cl', 'command_line', 'executable_path',
        'language', 'official', 'os_type', 'profile_path', 'useragent',
        'version', 'version_bitsize', 'version_modifier'];
    const versionDetails = Object.fromEntries(
        loadTimeDataKeys.map(key => [key, window.loadTimeData.getValue(key)]));

    const now = new Date();
    const fileName = `omnibox_batch_${ExportDelegate.getTimeStamp(now)}.json`;
    // If this data format changes, please roll schemaVersion.
    const batchData = {
      schemaKind: 'Omnibox Batch Export',
      schemaVersion: 3,
      dateCreated: now.toISOString(),
      author: '',
      description: '',
      authorTool: 'chrome://omnibox',
      batchName,
      versionDetails,
      variationInfo,
      pathInfo,
      appVersion: navigator.appVersion,
      batchExports
    };
    ExportDelegate.download_(batchData, fileName);
  }

  /**
   * Event handler for uploaded batch processing specifier data, kicks off
   * the processBatch asynchronous pipeline.
   * @param {!BatchSpecifier} processBatchData
   */
  processBatchData(processBatchData) {
    if (processBatchData.batchMode && processBatchData.batchQueryInputs &&
        processBatchData.batchName) {
      this.processBatch(
          processBatchData.batchQueryInputs, processBatchData.batchName);
    } else {
      const expected = {
        batchMode: "combined",
        batchName: "name for this batch of queries",
        batchQueryInputs: [
          {
            inputText: "example input text",
            cursorPosition: 18,
            resetAutocompleteController: false,
            cursorLock: false,
            zeroSuggest: false,
            preventInlineAutocomplete: false,
            preferKeyword: false,
            currentUrl: "",
            pageClassification: "4"
          }
        ],
      };
      console.error(`Invalid batch specifier data.  Expected format: \n${
          JSON.stringify(expected, null, 2)}`);
    }
  }

  exportClipboard() {
    navigator.clipboard.writeText(JSON.stringify(this.exportData_)).catch(
        error => console.error('unable to export to clipboard:', error));
  }

  exportFile() {
    const exportData = this.exportData_;
    const timeStamp = ExportDelegate.getTimeStamp();
    const fileName =
        `omnibox_debug_export_${exportData.queryInputs.inputText}_${timeStamp}.json`;
    ExportDelegate.download_(exportData, fileName);
  }

  /** @private @return {OmniboxExport} */
  get exportData_() {
    return {
      queryInputs: this.omniboxInput_.queryInputs,
      displayInputs: this.omniboxInput_.displayInputs,
      responsesHistory: this.omniboxOutput_.responsesHistory,
    };
  }

  /**
   * @private
   * @param {Object} object
   * @param {string} fileName
   */
  static download_(object, fileName) {
    const content = JSON.stringify(object, null, 2);
    const blob = new Blob([content], {type: 'application/json'});
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = fileName;
    a.click();
  }

  /**
    * @param {Date=} date
    * @return {string} A sortable timestamp string for use in filenames.
    */
  static getTimeStamp(date) {
    if (!date) {
      date = new Date();
    }
    const iso = date.toISOString();
    return iso.replace(/:/g, '').split('.')[0];
  }
}

/**
 * This is the minimum validation required to ensure no console errors.
 * Invalid importData that passes validation will be processed with a
 * best-attempt; e.g. if responses are missing 'relevance' values, then those
 * cells will be left blank.
 * @private
 * @param {OmniboxExport} importData
 * @return {boolean}
 */
function validateImportData_(importData) {
  const EXPECTED_FORMAT = {
    queryInputs: {},
    displayInputs: {},
    responsesHistory: [[{combinedResults: [], resultsByProvider: []}]]
  };
  const INVALID_MESSAGE = `Invalid import format; expected \n${
      JSON.stringify(EXPECTED_FORMAT, null, 2)};\n`;

  if (!importData) {
    console.error(INVALID_MESSAGE + 'received non object.');
    return false;
  }

  if (!importData.queryInputs || !importData.displayInputs) {
    console.error(
        INVALID_MESSAGE +
        'import missing objects queryInputs and displayInputs.');
    return false;
  }

  if (!Array.isArray(importData.responsesHistory)) {
    console.error(INVALID_MESSAGE + 'import missing array responsesHistory.');
    return false;
  }

  if (!importData.responsesHistory.every(Array.isArray)) {
    console.error(INVALID_MESSAGE + 'responsesHistory contains non arrays.');
    return false;
  }

  if (!importData.responsesHistory.every(
          responses => responses.every(
              ({combinedResults, resultsByProvider}) =>
                  Array.isArray(combinedResults) &&
                  Array.isArray(resultsByProvider)))) {
    console.error(
        INVALID_MESSAGE +
        'responsesHistory items\' items missing combinedResults and ' +
        'resultsByProvider arrays.');
    return false;
  }

  return true;
}
})();
