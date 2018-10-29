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

(function () {
  /**
   * @type {number} the value for cursor position we sent with the most
   *     recent request.  We need to remember this in order to display it
   *     in the output; otherwise it's hard or impossible to determine
   *     from screen captures or print-to-PDFs.
   */
  let cursorPosition = -1;

  /**
   * Tracks and aggregates responses from the C++ autocomplete controller.
   * Typically, the C++ controller returns 3 sets of results per query, unless
   * a new query is submitted before all 3 responses. OutputController also
   * triggers appending to and clearing of OmniboxOutput when appropriate (e.g.,
   * upon receiving a new response or a change in display inputs).
   */
  class OutputController {
    constructor() {
      /** @private {!Array<mojom.OmniboxResult>} */
      this.outputResultsGroups_ = [];
    }

    clear() {
      this.outputResultsGroups_ = [];
      omniboxOutput.clearOutput();
    }

    /*
     * Adds a new response to the page. If we're not displaying incomplete
     * results, we clear the page and display only the new result. If we are
     * displaying incomplete results, then this is more efficient than refresh,
     * as there's no need to clear and re-add previous results.
     */
    /** @param {!mojom.OmniboxResult} response A response from C++ autocomplete controller */
    add(response) {
      this.outputResultsGroups_.push(response);
      if (!omniboxInputs.$$('show-incomplete-results').checked)
        omniboxOutput.clearOutput();
      addResultToOutput(
          this.outputResultsGroups_[this.outputResultsGroups_.length - 1]);
    }

    /*
     * Refreshes all results. We only display the last (most recent) entry
     * unless incomplete results is enabled.
     */
    refresh() {
      omniboxOutput.clearOutput();
      if (omniboxInputs.$$('show-incomplete-results').checked) {
        this.outputResultsGroups_.forEach(addResultToOutput);
      } else if (this.outputResultsGroups_.length) {
        addResultToOutput(
            this.outputResultsGroups_[this.outputResultsGroups_.length - 1]);
      }
    }
  }

  /**
   * Appends some human-readable information about the provided
   * autocomplete result to the HTML node with id omnibox-debug-text.
   * The current human-readable form is a few lines about general
   * autocomplete result statistics followed by a table with one line
   * for each autocomplete match.  The input parameter is an OmniboxResultMojo.
   */
  function addResultToOutput(result) {
    const resultsGroup = new omnibox_output.OutputResultsGroup(result).render(
        omniboxInputs.$$('show-details').checked,
        omniboxInputs.$$('show-incomplete-results').checked,
        omniboxInputs.$$('show-all-providers').checked);
    omniboxOutput.addOutput(resultsGroup);
  }

  class BrowserProxy {
    constructor() {
      /** @private {!mojom.OmniboxPageHandlerPtr} */
      this.pagehandlePtr_ = new mojom.OmniboxPageHandlerPtr;
      Mojo.bindInterface(
          mojom.OmniboxPageHandler.name,
          mojo.makeRequest(this.pagehandlePtr_).handle);
      const client = new mojom.OmniboxPagePtr;
      // NOTE: Need to keep a global reference to the |binding_| such that it is
      // not garbage collected, which causes the pipe to close and future calls
      // from C++ to JS to get dropped.
      /** @private {!mojo.Binding} */
      this.binding_ =
          new mojo.Binding(mojom.OmniboxPage, this, mojo.makeRequest(client));
      this.pagehandlePtr_.setClientPage(client);
    }

    /**
     * Extracts the input text from the text field and sends it to the
     * C++ portion of chrome to handle.  The C++ code will iteratively
     * call handleNewAutocompleteResult as results come in.
     */
    makeRequest(inputString,
                cursorPosition,
                preventInlineAutocomplete,
                preferKeyword,
                pageClassification) {
      outputController.clear();
      // Then, call chrome with a five-element list:
      // - first element: the value in the text box
      // - second element: the location of the cursor in the text box
      // - third element: the value of prevent-inline-autocomplete
      // - forth element: the value of prefer-keyword
      // - fifth element: the value of page-classification
      this.pagehandlePtr_.startOmniboxQuery(
          inputString,
          cursorPosition,
          preventInlineAutocomplete,
          preferKeyword,
          pageClassification);
    }

    handleNewAutocompleteResult(response) {
      outputController.add(response);
    }
  }

  /** @type {BrowserProxy} */
  const browserProxy = new BrowserProxy();
  /** @type {OmniboxInputs} */
  let omniboxInputs;
  /** @type {omnibox_output.OmniboxOutput} */
  let omniboxOutput;
  /** @type {OutputController} */
  const outputController = new OutputController();

  document.addEventListener('DOMContentLoaded', () => {
    omniboxInputs = /** @type {!OmniboxInputs} */ ($('omnibox-inputs'));
    omniboxOutput =
        /** @type {!omnibox_output.OmniboxOutput} */ ($('omnibox-output'));
    omniboxInputs.addEventListener('query-inputs-changed', event =>
        browserProxy.makeRequest(
            event.detail.inputText,
            event.detail.cursorPosition,
            event.detail.preventInlineAutocomplete,
            event.detail.preferKeyword,
            event.detail.pageClassification
        ));
    omniboxInputs.addEventListener('display-inputs-changed',
        outputController.refresh.bind(outputController));
  });
})();
