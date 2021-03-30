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
  PREIVOUS_BUTTON: 'previous',
  INTRO_CONTENT: 'intro',
  ASSIGN_SELECT_CONTENT: 'assign-select',
};

/**
 * The IDs of each page in the setup flow.
 * @enum {number}
 */
const SASetupPageId = {
  INTRO: 0,
  ASSIGN_SELECT: 1,
  AUTO_SCAN_EXPLANATION: 2,
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
    SASetupElement.EXIT_BUTTON, SASetupElement.PREIVOUS_BUTTON,
    SASetupElement.ASSIGN_SELECT_CONTENT
  ]
};

Polymer({
  is: 'settings-switch-access-setup-guide-dialog',

  behaviors: [
    I18nBehavior,
  ],

  properties: {
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
   * Determines what page is shown next, given the current page ID and other
   * state.
   * @return {!SASetupPageId}
   * @private
   */
  getNextPageId_() {
    return SASetupPageId.ASSIGN_SELECT;
  },

  /**
   * Returns what page was shown previously, given the current page ID.
   * @return {!SASetupPageId}
   * @private
   */
  getPreviousPageId_() {
    return SASetupPageId.INTRO;
  },

  /** @private */
  onExitClick_() {
    this.$.switchAccessSetupGuideDialog.close();
  },

  /** @private */
  onNextClick_() {
    this.loadPage_(this.getNextPageId_());
  },

  /** @private */
  onPreviousClick_() {
    this.loadPage_(this.getPreviousPageId_());
  },

  /** @private */
  onBluetoothClick_() {
    settings.Router.getInstance().navigateTo(settings.routes.BLUETOOTH_DEVICES);
  }
});
