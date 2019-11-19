// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Class to manage SwitchAccess and interact with other controllers.
 * @implements {SwitchAccessInterface}
 */
class SwitchAccess {
  static initialize() {
    window.switchAccess = new SwitchAccess();
  }

  static get() {
    return window.switchAccess;
  }

  /** @private */
  constructor() {
    /**
     * User commands.
     * @private {Commands}
     */
    this.commands_ = null;

    /**
     * User preferences.
     * @private {SwitchAccessPreferences}
     */
    this.switchAccessPreferences_ = null;

    /**
     * Handles changes to auto-scan.
     * @private {AutoScanManager}
     */
    this.autoScanManager_ = null;

    /**
     * Handles interactions with the accessibility tree, including moving to and
     * selecting nodes.
     * @private {NavigationManager}
     */
    this.navigationManager_ = null;

    /**
     * Callback for testing use only.
     * @private {?function()}
     */
    this.onMoveForwardForTesting_ = null;

    /**
     * Callback that is called once the navigation manager is initialized.
     * Used to setup communications with the menu panel.
     * @private {?function()}
     */
    this.navReadyCallback_ = null;

    /**
     * Feature flag controlling improvement of text input capabilities.
     * @private {boolean}
     */
    this.enableImprovedTextInput_ = false;

    /**
     * The automation node for the back button.
     * @private {chrome.automation.AutomationNode}
     */
    this.backButtonAutomationNode_;

    /**
     * The desktop node.
     * @private {chrome.automation.AutomationNode}
     */
    this.desktop_;

    this.init_();
  }

  /**
   * Set up preferences, controllers, and event listeners.
   * @private
   */
  init_() {
    chrome.commandLinePrivate.hasSwitch(
        'enable-experimental-accessibility-switch-access-text', (result) => {
          this.enableImprovedTextInput_ = result;
        });

    this.commands_ = new Commands(this);
    this.autoScanManager_ = new AutoScanManager(this);
    this.switchAccessPreferences_ =
        new SwitchAccessPreferences(this, this.onPrefsReady_.bind(this));

    chrome.automation.getDesktop(function(desktop) {
      this.navigationManager_ = new NavigationManager(desktop);
      this.desktop_ = desktop;
      this.findBackButtonNode_();

      if (this.navReadyCallback_) {
        this.navReadyCallback_();
      }
    }.bind(this));
  }

  /**
   * Open and jump to the Switch Access menu.
   * @override
   */
  enterMenu() {
    if (this.navigationManager_) {
      this.navigationManager_.enterMenu();
    }
  }

  /**
   * Move to the next interesting node.
   * @override
   */
  moveForward() {
    if (this.navigationManager_) {
      this.navigationManager_.moveForward();
    }
    if (this.onMoveForwardForTesting_) {
      this.onMoveForwardForTesting_();
    }
  }

  /**
   * Move to the previous interesting node.
   * @override
   */
  moveBackward() {
    if (this.navigationManager_) {
      this.navigationManager_.moveBackward();
    }
  }

  /**
   * Perform the default action on the current node.
   * @override
   */
  selectCurrentNode() {
    if (this.navigationManager_) {
      this.navigationManager_.selectCurrentNode();
    }
  }

  /**
   * Returns whether or not the feature flag
   * for improved text input is enabled.
   * @return {boolean}
   * @override
   */
  improvedTextInputEnabled() {
    return this.enableImprovedTextInput_;
  }

  /**
   * Restarts auto-scan if it is enabled.
   * @override
   */
  restartAutoScan() {
    this.autoScanManager_.restartIfRunning();
  }

  /**
   * Sets whether the current node is in the virtual keyboard.
   * @param {boolean} inKeyboard
   * @override
   */
  setInKeyboard(inKeyboard) {
    this.autoScanManager_.setInKeyboard(inKeyboard);
  }

  /**
   * Handle a change in user preferences.
   * @override
   * @param {!Object} changes
   */
  onPreferencesChanged(changes) {
    for (const key of Object.keys(changes)) {
      switch (key) {
        case SAConstants.Preference.AUTO_SCAN_ENABLED:
          this.autoScanManager_.setEnabled(changes[key]);
          break;
        case SAConstants.Preference.AUTO_SCAN_TIME:
          this.autoScanManager_.setDefaultScanTime(changes[key]);
          break;
        case SAConstants.Preference.AUTO_SCAN_KEYBOARD_TIME:
          this.autoScanManager_.setKeyboardScanTime(changes[key]);
          break;
      }
    }
  }

  /**
   * Returns whether prefs have initially loaded or not.
   * @return {boolean}
   * @override
   */
  prefsAreReady() {
    return this.switchAccessPreferences_.isReady();
  }

  /**
   * Set the value of the preference |name| to |value| in chrome.storage.sync.
   * Once the storage is set, the Switch Access preferences/behavior are
   * updated.
   *
   * @override
   * @param {SAConstants.Preference} name
   * @param {boolean|number} value
   */
  setPreference(name, value) {
    this.switchAccessPreferences_.setPreference(name, value);
  }

  /**
   * Get the boolean value for the given name. Will throw an error if the
   * value associated with |name| is not a boolean, or undefined.
   *
   * @override
   * @param  {SAConstants.Preference} name
   * @return {boolean}
   */
  getBooleanPreference(name) {
    return this.switchAccessPreferences_.getBooleanPreference(name);
  }

  /**
   * Get the string value for the given name. Will throw an error if the
   * value associated with |name| is not a string, or is undefined.
   *
   * @override
   * @param {SAConstants.Preference} name
   * @return {string}
   */
  getStringPreference(name) {
    return this.switchAccessPreferences_.getStringPreference(name);
  }

  /**
   * Get the number value for the given name. Will throw an error if the
   * value associated with |name| is not a number, or undefined.
   *
   * @override
   * @param  {SAConstants.Preference} name
   * @return {number}
   */
  getNumberPreference(name) {
    return this.switchAccessPreferences_.getNumberPreference(name);
  }

  /**
   * Get the number value for the given name, or |null| if none exists.
   *
   * @override
   * @param  {SAConstants.Preference} name
   * @return {number|null}
   */
  getNumberPreferenceIfDefined(name) {
    return this.switchAccessPreferences_.getNumberPreferenceIfDefined(name);
  }

  /**
   * Sets up the connection between the menuPanel and menuManager.
   * @param {!PanelInterface} menuPanel
   * @return {MenuManager}
   */
  connectMenuPanel(menuPanel) {
    // Because this may be called before init_(), check if navigationManager_
    // is initialized.
    if (this.navigationManager_) {
      return this.navigationManager_.connectMenuPanel(menuPanel);
    }

    // If not, set navReadyCallback_ to have the menuPanel try again.
    this.navReadyCallback_ = menuPanel.connectToBackground.bind(menuPanel);
    return null;
  }

  /**
   * Notifies managers that the preferences have initially loaded.
   */
  onPrefsReady_() {
    this.autoScanManager_.onPrefsReady();
    if (this.navigationManager_) {
      this.navigationManager_.onPrefsReady();
    }
  }

  /** @return {chrome.automation.AutomationNode} */
  getBackButtonAutomationNode() {
    if (!this.backButtonAutomationNode_) {
      this.findBackButtonNode_();
      if (!this.backButtonAutomationNode_) {
        console.log('Error: unable to find back button');
      }
    }
    return this.backButtonAutomationNode_;
  }

  /**
   * Looks for the back button node.
   */
  findBackButtonNode_() {
    if (!this.desktop_) {
      return;
    }
    this.backButtonAutomationNode_ =
        new AutomationTreeWalker(
            this.desktop_, constants.Dir.FORWARD,
            {visit: (node) => node.htmlAttributes.id === SAConstants.BACK_ID})
            .next()
            .node;
  }

  /*
   * Creates and records the specified error.
   * @param {SAConstants.ErrorType} errorType
   * @param {string} errorString
   * @return {!Error}
   */
  static error(errorType, errorString) {
    let errorTypeCountForUMA = Object.keys(SAConstants.ErrorType).length;
    chrome.metricsPrivate.recordEnumerationValue(
        'Accessibility.CrosSwitchAccess.Error', errorType,
        errorTypeCountForUMA);
    return new Error(errorString);
  }

  /**
   * Prints out the current Switch Access tree for debugging.
   * @param {boolean=} wholeTree whether to print the whole tree, or the current
   * focus.
   * @return {SARootNode|undefined}
   */
  getTreeForDebugging(wholeTree = false) {
    if (this.navigationManager_) {
      return this.navigationManager_.getTreeForDebugging(wholeTree);
    }
  }
}
