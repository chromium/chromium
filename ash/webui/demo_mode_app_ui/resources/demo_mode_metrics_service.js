// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The page/pillar name for the pages in the Highlights app.
 * @enum {string}
 */
export const Page = {
  HOME: 'Home',
  EASY: 'Easy',
  FAST: 'Fast',
  POWERFUL: 'Powerful',
  SECURE: 'Secure',
  DISPLAY: 'Display',
  KEYBOARD: 'Keyboard',
  GRAPHICS: 'Graphics',
  GAME: 'Game',
  PERIPHERALS: 'Peripherals',
};

/**
 * The buttons in each pillar page of the Highlights app.
 * @enum {string}
 */
export const PillarButton = {
  NEXT: 'Next',
  PREVIOUS: 'Previous',
};

/**
 * A map between the Page in this js file and DemoModeHighlightsAction enum in
 * the UMA enums.xml.
 */
const FirstInteractionActionMap = new Map([
  [Page.HOME, 0],
  [Page.EASY, 1],
  [Page.FAST, 2],
  [Page.POWERFUL, 3],
  [Page.SECURE, 4],
  [Page.DISPLAY, 5],
  [Page.KEYBOARD, 6],
  [Page.GRAPHICS, 7],
  [Page.GAME, 8],
  [Page.PERIPHERALS, 9],
  ['MAX_VALUE', 10],
]);

/**
 * Provides interfaces for emitting metrics from demo mode apps to UMA.
 */
class DemoMetricsService {
  constructor() {
    this.firstInteractionRecorded = false;
  }

  // Record the action that the user breaks the current Attract Loop.
  recordAttractLoopBreak() {
    chrome.metricsPrivateIndividualApis.recordUserAction(
        'DemoMode_AttractLoop_Break');
  }

  /**
   * Record the first action of current user.
   * @param {Page} action
   */
  recordFirstInteraction(action) {
    if (!this.firstInteractionRecorded) {
      chrome.metricsPrivateIndividualApis.recordEnumerationValue(
          'DemoMode.Highlights.FirstInteraction',
          FirstInteractionActionMap.get(action),
          FirstInteractionActionMap.get('MAX_VALUE'));
      this.firstInteractionRecorded = true;
    }
  }

  /**
   * Record the button clicks in home page of the current user.
   * @param {Page} page
   */
  recordHomePageButtonClick(page) {
    chrome.metricsPrivateIndividualApis.recordUserAction(
        'DemoMode_Highlights_HomePage_Click_' + page + 'Button');
    this.recordFirstInteraction(page);
  }

  /**
   * Record the button clicks in home page of the current user.
   * @param {Page} page
   */
  recordNavbarButtonClick(page) {
    chrome.metricsPrivateIndividualApis.recordUserAction(
        'DemoMode_Highlights_Navbar_Click_' + page + 'Button');
  }

  /**
   * Record the button click in pillar pages of the current user.
   * @param {PillarButton} pillarButton
   */
  recordPillarPageButtonClick(pillarButton) {
    chrome.metricsPrivateIndividualApis.recordUserAction(
        'DemoMode_Highlights_PillarPage_Click_' + pillarButton + 'Button');
  }

  /**
   * Record the duration of the user staying on the page.
   * @param {Page} page
   * @param {number} durationInMilliseconds
   */
  recordPageViewDuration(page, durationInMilliseconds) {
    chrome.metricsPrivateIndividualApis.recordMediumTime(
        'DemoMode.Highlights.PageStayDuration.' + page + 'Page',
        durationInMilliseconds);
  }
}

export const metricsService = new DemoMetricsService();