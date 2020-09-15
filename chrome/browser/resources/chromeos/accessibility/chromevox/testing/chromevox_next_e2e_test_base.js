// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['chromevox_e2e_test_base.js']);
GEN_INCLUDE(['mock_feedback.js']);

/**
 * Base test fixture for ChromeVox Next end to end tests.
 * These tests are identical to ChromeVoxE2ETests except for performing the
 * necessary setup to run ChromeVox Next.
 */
ChromeVoxNextE2ETest = class extends ChromeVoxE2ETest {
  constructor() {
    super();

    if (this.runtimeDeps.length > 0) {
      chrome.extension.getViews().forEach(function(w) {
        this.runtimeDeps.forEach(function(dep) {
          if (w[dep]) {
            window[dep] = w[dep];
          }
        }.bind(this));
      }.bind(this));
    }

    // For tests, enable announcement of events we trigger via automation.
    DesktopAutomationHandler.announceActions = true;

    this.originalOutputContextValues_ = {};
    for (const role in Output.ROLE_INFO_) {
      this.originalOutputContextValues_[role] =
          Output.ROLE_INFO_[role]['outputContextFirst'];
    }
  }

  /** @return {!MockFeedback} */
  createMockFeedback() {
    const mockFeedback =
        new MockFeedback(this.newCallback(), this.newCallback.bind(this));
    mockFeedback.install();
    return mockFeedback;
  }

  /**
   * Create a function which performs the command |cmd|.
   * @param {string} cmd
   * @return {function(): void}
   */
  doCmd(cmd) {
    return () => {
      CommandHandler.onCommand(cmd);
    };
  }

  /**
   * Dependencies defined on a background window other than this one.
   * @type {!Array<string>}
   */
  get runtimeDeps() {
    return [];
  }

  /** @override */
  runWithLoadedTree(doc, callback, opt_params = {}) {
    if (opt_params.returnPage === undefined) {
      opt_params.returnPage = true;
    }

    callback = this.newCallback(callback);
    const wrappedCallback = (node) => {
      CommandHandler.onCommand('nextObject');
      callback(node);
    };

    super.runWithLoadedTree(doc, wrappedCallback, opt_params);
  }

  /**
   * Forces output to place context utterances at the end of output. This eases
   * rebaselining when changing context ordering for a specific role.
   */
  forceContextualLastOutput() {
    for (const role in Output.ROLE_INFO_) {
      Output.ROLE_INFO_[role]['outputContextFirst'] = undefined;
    }
  }

  /**
   * Forces output to place context utterances at the beginning of output.
   */
  forceContextualFirstOutput() {
    for (const role in Output.ROLE_INFO_) {
      Output.ROLE_INFO_[role]['outputContextFirst'] = true;
    }
  }

  /** Resets contextual output values to their defaults. */
  resetContextualOutput() {
    for (const role in Output.ROLE_INFO_) {
      Output.ROLE_INFO_[role]['outputContextFirst'] =
          this.originalOutputContextValues_[role];
    }
  }
};
