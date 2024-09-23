// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['../../common/testing/e2e_test_base.js']);
GEN_INCLUDE(['../../common/testing/assert_additions.js']);
GEN_INCLUDE(['common.js']);
GEN_INCLUDE(['mock_feedback.js']);

/**
 * Base test fixture for ChromeVox end to end tests.
 * These tests run against production ChromeVox inside of the extension's
 * background context.
 */
ChromeVoxE2ETest = class extends E2ETestBase {
  constructor() {
    super();

    if (this.runtimeDeps.length > 0) {
      chrome.extension.getViews().forEach(view => {
        this.runtimeDeps.forEach(dep => {
          if (view[dep]) {
            window[dep] = view[dep];
          }
        });
      });
    }

    this.originalOutputContextValues_ = {};
  }

  /** @override */
  setUp() {
    globalThis.doCmd = this.doCmd;
    globalThis.doGesture = this.doGesture;

    super.setUp();
  }

  /** @override */
  testGenCppIncludes() {
    super.testGenCppIncludes();
    GEN(`
  #include "extensions/common/extension_l10n_util.h"
      `);
  }

  /** @override */
  testGenPreamble() {
    super.testGenPreamble();
    GEN(`
    auto allow = extension_l10n_util::AllowGzippedMessagesAllowedForTest();
    base::OnceClosure load_cb =
        base::BindOnce(&ash::AccessibilityManager::EnableSpokenFeedback,
            base::Unretained(ash::AccessibilityManager::Get()),
            true);
      `);

    super.testGenPreambleCommon(
        'kChromeVoxExtensionId', ChromeVoxE2ETest.prototype.failOnConsoleError);
  }

  /**
   * Creates a mock feedback object. Please, note that created mock also gets
   * installed, i.e. it starts collecting tts/braille/earcons output
   * immediately, which can affect your expectations if the environment has its
   * own (other than induced by your test scenario) announcements.
   * @return {!MockFeedback}
   */
  createMockFeedback() {
    const mockFeedback = new MockFeedback(this.newCallback());
    mockFeedback.install();
    return mockFeedback;
  }

  /**
   * Create a mock event object.
   * @param {number} keyCode
   * @param {{altGraphKey: boolean=,
   *         altKey: boolean=,
   *         ctrlKey: boolean=,
   *         metaKey: boolean=,
   *         searchKeyHeld: boolean=,
   *         shiftKey: boolean=,
   *         stickyMode: boolean=,
   *         prefixKey: boolean=}=} opt_modifiers
   * @return {Object} The mock event.
   */
  createMockKeyEvent(keyCode, opt_modifiers) {
    const modifiers = opt_modifiers || {};
    const keyEvent = {};
    keyEvent.keyCode = keyCode;
    for (const key in modifiers) {
      keyEvent[key] = modifiers[key];
    }
    keyEvent.preventDefault = () => {};
    keyEvent.stopPropagation = () => {};
    return keyEvent;
  }

  /**
   * Create a function which performs the command |cmd|.
   * @param {string} cmd
   * @return {function(): void}
   */
  doCmd(cmd) {
    return () => CommandHandlerInterface.instance.onCommand(cmd);
  }

  /**
   * Create a function which performs the gesture |gesture|.
   * @param {chrome.accessibilityPrivate.Gesture} gesture
   * @param {number} opt_x
   * @param {number} opt_y
   * @return {function(): void}
   */
  doGesture(gesture, opt_x, opt_y) {
    return () => GestureCommandHandler.instance.onAccessibilityGesture_(
               gesture, opt_x, opt_y);
  }

  /**
   * Dependencies defined on a background window other than this one.
   * @type {!Array<string>}
   */
  get runtimeDeps() {
    return [];
  }

  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();

    // For tests, enable announcement of events we trigger via automation.
    BaseAutomationHandler.announceActions = true;

    for (const role in OutputRoleInfo) {
      this.originalOutputContextValues_[role] =
          OutputRoleInfo[role]['contextOrder'];
    }
    await ChromeVoxState.ready();
  }

  /** @override */
  async runWithLoadedTree(doc, opt_params = {}) {
    const rootWebArea = await super.runWithLoadedTree(doc, opt_params);
    CommandHandlerInterface.instance.onCommand('nextObject');
    return rootWebArea;
  }

  /**
   * Forces output to place context utterances at the end of output. This eases
   * rebaselining when changing context ordering for a specific role.
   */
  forceContextualLastOutput() {
    for (const role in OutputRoleInfo) {
      OutputRoleInfo[role]['contextOrder'] = OutputContextOrder.LAST;
    }
  }

  /**
   * Forces output to place context utterances at the beginning of output.
   */
  forceContextualFirstOutput() {
    for (const role in OutputRoleInfo) {
      OutputRoleInfo[role]['contextOrder'] = OutputContextOrder.FIRST;
    }
  }

  /** Resets contextual output values to their defaults. */
  resetContextualOutput() {
    for (const role in OutputRoleInfo) {
      OutputRoleInfo[role]['contextOrder'] =
          this.originalOutputContextValues_[role];
    }
  }
};

// TODO: wasm logs errors if it takes too long to load (e.g. liblouis wasm).
// Separately, LibLouis also logs errors.
// See https://crbug.com/1170991.
ChromeVoxE2ETest.prototype.failOnConsoleError = false;
