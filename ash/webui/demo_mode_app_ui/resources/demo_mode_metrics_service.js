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
  PERFORMANCE: 'Performance',
  APPS: 'Apps',
  CHROMEOS: 'ChromeOS',
};

/**
 * The name for the details pages in the Highlights app.
 * @enum {string}
 */
export const DetailsPage = {
  ADOBE: 'Adobe',
  BATTERY: 'Battery',
  COMPARISON: 'Comparison',
  DISPLAY_ENTERTAINMENT: 'DisplayEntertainment',
  DISPLAY_PERFORMANCE: 'DisplayPerformance',
  ENTERTAINMENT_APPS: 'EntertainmentApps',
  GOOGLE_APPS: 'GoogleApps',
  LUMAFUSION: 'LumaFusion',
  MESSAGING: 'Messaging',
  MOBILE_GAMING: 'MobileGaming',
  MS_365_APPS: 'MS365Apps',
  MS_OFFICE: 'MSOffice',
  NEARBY_SHARE: 'NearbyShare',
  OFFLINE_MODE: 'OfflineMode',
  PC_CONSOLE_GAMING: 'PCConsoleGaming',
  PHOTOS: 'Photos',
  PROCESSOR: 'Processor',
  STORAGE: 'Storage',
  SWITCHING: 'Switching',
  VIDEO_CALL: 'VideoCall',
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
  [Page.PERFORMANCE, 10],
  [Page.APPS, 11],
  [Page.CHROMEOS, 12],
  ['MAX_VALUE', 13],
]);

/**
 * Provides interfaces for emitting metrics from demo mode apps to UMA.
 *
 * Note: DemoMode.Highlights.* metrics and actions are recorded via
 * runtime-downloaded content that is not checked into Chromium. Please do not
 * delete this code, even if it looks like there's no production references in
 * Chromium, without first consulting the Demo Mode team.
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
   * Record the timestamp (i.e. milliseconds from the beginning of the Attract
   * Loop video) at which the user broke the Attract Loop.
   * @param timestampInMilliseconds
   */
  recordAttractLoopBreakTimestamp(timestampInMilliseconds) {
    chrome.metricsPrivateIndividualApis.recordMediumTime(
        'DemoMode.AttractLoop.Timestamp',
        timestampInMilliseconds,
    );
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
    this.recordFirstInteraction(page);
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

  /**
   * Record the details page clicked by the current user
   * @param {DetailsPage} detailsPage
   */
  recordDetailsPageClicked(detailsPage) {
    chrome.metricsPrivateIndividualApis.recordUserAction(
        'DemoMode_Highlights_DetailsPage_Clicked_' + detailsPage + 'Button');
  }

  /**
   * Record the duration of the user staying on a details page
   * @param {DetailsPage} detailsPage
   */
  recordDetailsPageViewDuration(detailsPage, durationInMilliseconds) {
    chrome.metricsPrivateIndividualApis.recordMediumTime(
        'DemoMode.Highlights.DetailsPageStayDuration.' + detailsPage + 'Page',
        durationInMilliseconds);
  }
}



export const metricsService = new DemoMetricsService();