// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This dialog walks a user through the flow of setting up Switch
 * Access.
 */

/**
 * Elements that can be hidden or shown for each setup page.
 * The string value should match the element ID in the HTML.
 * @enum {string}
 */
const SASetupElement = {
  BLUETOOTH_BUTTON: 'bluetooth',
  EXIT_BUTTON: 'exit',
  NEXT_BUTTON: 'next',
  PREVIOUS_BUTTON: 'previous',
  INTRO_CONTENT: 'intro',
  ASSIGN_SELECT_CONTENT: 'assign-select',
  AUTO_SCAN_ENABLED_CONTENT: 'auto-scan-enabled',
  CHOOSE_SWITCH_COUNT_CONTENT: 'choose-switch-count',
};

/**
 * The IDs of each page in the setup flow.
 * @enum {number}
 */
const SASetupPageId = {
  INTRO: 0,
  ASSIGN_SELECT: 1,
  AUTO_SCAN_ENABLED: 2,
  CHOOSE_SWITCH_COUNT: 3,
  AUTO_SCAN_SPEED: 4,
  ASSIGN_NEXT: 5,
  ASSIGN_PREVIOUS: 6,
  TIC_TAC_TOE: 7,
  DONE: 8,
};


/**
 * Defines what is visible onscreen for a given page of the setup guide.
 * @typedef {{titleId: string, visibleElements: !Array<SASetupElement>}}
 */
let SASetupPage;

/**
 * A dictionary of all of the dialog pages.
 * @type {Object<SASetupPageId, SASetupPage>}
 */
const SASetupPageList = {};

SASetupPageList[SASetupPageId.INTRO] = {
  titleId: 'switchAccessSetupIntroTitle',
  visibleElements: [
    SASetupElement.BLUETOOTH_BUTTON, SASetupElement.EXIT_BUTTON,
    SASetupElement.NEXT_BUTTON, SASetupElement.INTRO_CONTENT
  ]
};

SASetupPageList[SASetupPageId.ASSIGN_SELECT] = {
  titleId: 'switchAccessSetupIntroTitle',
  visibleElements: [
    SASetupElement.NEXT_BUTTON, SASetupElement.PREVIOUS_BUTTON,
    SASetupElement.ASSIGN_SELECT_CONTENT
  ]
};

SASetupPageList[SASetupPageId.AUTO_SCAN_ENABLED] = {
  titleId: 'switchAccessSetupAutoScanEnabledTitle',
  visibleElements: [
    SASetupElement.NEXT_BUTTON, SASetupElement.PREVIOUS_BUTTON,
    SASetupElement.AUTO_SCAN_ENABLED_CONTENT
  ]
};

SASetupPageList[SASetupPageId.CHOOSE_SWITCH_COUNT] = {
  titleId: 'switchAccessSetupChooseSwitchCountTitle',
  visibleElements: [
    SASetupElement.EXIT_BUTTON, SASetupElement.PREVIOUS_BUTTON,
    SASetupElement.CHOOSE_SWITCH_COUNT_CONTENT
  ]
};

Polymer({
  is: 'settings-switch-access-setup-guide-dialog',

  behaviors: [
    I18nBehavior,
    PrefsBehavior,
  ],

  properties: {
    autoScanPreviouslyEnabled_: {type: Boolean, value: false},
    currentPageId_: {
      type: Number,
      value: SASetupPageId.INTRO,
    }
  },

  /**
   * @param {SASetupPageId} id
   * @private
   */
  loadPage_(id) {
    const newPage = SASetupPageList[id];
    this.$.title.textContent = this.i18n(newPage.titleId);

    for (const element of Object.values(SASetupElement)) {
      this['$'][element]['hidden'] = !newPage.visibleElements.includes(element);
    }

    this.currentPageId_ = id;
  },

  /**
   * Determines what page is shown next, from the current page ID and other
   * state.
   * @return {!SASetupPageId}
   * @private
   */
  getNextPageId_() {
    switch (this.currentPageId_) {
      case SASetupPageId.INTRO:
        return SASetupPageId.ASSIGN_SELECT;
      case SASetupPageId.ASSIGN_SELECT:
        return SASetupPageId.AUTO_SCAN_ENABLED;
      case SASetupPageId.AUTO_SCAN_ENABLED:
      default:
        return SASetupPageId.CHOOSE_SWITCH_COUNT;
    }
  },

  /**
   * Returns what page was shown previously from the current page ID.
   * @return {!SASetupPageId}
   * @private
   */
  getPreviousPageId_() {
    switch (this.currentPageId_) {
      case SASetupPageId.CHOOSE_SWITCH_COUNT:
        return SASetupPageId.AUTO_SCAN_ENABLED;
      case SASetupPageId.AUTO_SCAN_ENABLED:
        return SASetupPageId.ASSIGN_SELECT;
      case SASetupPageId.ASSIGN_SELECT:
      default:
        return SASetupPageId.INTRO;
    }
  },

  /** @private */
  onExitClick_() {
    this.$.switchAccessSetupGuideDialog.close();
  },

  /** @private */
  onNextClick_() {
    this.loadPage_(this.getNextPageId_());

    // Enable auto-scan when we reach that page of the setup guide.
    if (this.currentPageId_ === SASetupPageId.AUTO_SCAN_ENABLED) {
      this.autoScanPreviouslyEnabled_ = /** @type {boolean} */ (
          this.getPref('settings.a11y.switch_access.auto_scan.enabled').value);
      chrome.settingsPrivate.setPref(
          'settings.a11y.switch_access.auto_scan.enabled', true);
    }
  },

  /** @private */
  onPreviousClick_() {
    // Disable auto-scan, if we enabled it as part of the setup guide.
    if (this.currentPageId_ === SASetupPageId.AUTO_SCAN_ENABLED &&
        !this.autoScanPreviouslyEnabled_) {
      chrome.settingsPrivate.setPref(
          'settings.a11y.switch_access.auto_scan.enabled', false);
    }

    this.loadPage_(this.getPreviousPageId_());
  },

  /** @private */
  onBluetoothClick_() {
    settings.Router.getInstance().navigateTo(settings.routes.BLUETOOTH_DEVICES);
  }
});
