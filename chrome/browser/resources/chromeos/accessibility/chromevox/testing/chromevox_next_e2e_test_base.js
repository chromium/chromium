// Copyright 2014 The Chromium Authors
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
  /** @param {boolean=} opt_isCommonClass Disables ChromeVox specific code */
  constructor(opt_isCommonClass) {
    super();
    this.isCommonClass = opt_isCommonClass || false;

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
    window.EventType = chrome.automation.EventType;
    window.RoleType = chrome.automation.RoleType;
    window.TreeChangeType = chrome.automation.TreeChangeType;
    window.doCmd = this.doCmd;
    window.doGesture = this.doGesture;
    window.Gesture = chrome.accessibilityPrivate.Gesture;

    super.setUp();
  }

  /** @return {!MockFeedback} */
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
    const modifiers = opt_modifiers === undefined ? {} : opt_modifiers;
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
    return () => {
      CommandHandlerInterface.instance.onCommand(cmd);
    };
  }

  /**
   * Create a function which performs the gesture |gesture|.
   * @param {chrome.accessibilityPrivate.Gesture} gesture
   * @param {number} opt_x
   * @param {number} opt_y
   * @return {function(): void}
   */
  doGesture(gesture, opt_x, opt_y) {
    return () => GestureCommandHandler.instance_.onAccessibilityGesture_(
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
    if (!this.isCommonClass) {
      // Alphabetical based on file path.
      await importModule(
          'BaseAutomationHandler',
          '/chromevox/background/base_automation_handler.js');
      await importModule(
          'CommandHandler', '/chromevox/background/command_handler.js');
      await importModule(
          'CommandHandlerInterface',
          '/chromevox/background/command_handler_interface.js');
      await importModule(
          'GestureCommandHandler',
          '/chromevox/background/gesture_command_handler.js');
      await importModule(
          'OutputRoleInfo', '/chromevox/background/output/output_role_info.js');
      await importModule(
          'OutputContextOrder', '/chromevox/background/output/output_types.js');

      // For tests, enable announcement of events we trigger via automation.
      BaseAutomationHandler.announceActions = true;

      for (const role in OutputRoleInfo) {
        this.originalOutputContextValues_[role] =
            OutputRoleInfo[role]['contextOrder'];
      }
    }
  }

  /** @override */
  async runWithLoadedTree(doc, opt_params = {}) {
    const rootWebArea = await super.runWithLoadedTree(doc, opt_params);
    if (!this.isCommonClass) {
      CommandHandlerInterface.instance.onCommand('nextObject');
    }
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
