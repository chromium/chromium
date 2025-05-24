// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The page/pillar name for the pages in the Highlights app.
 */
export enum Page {
  HOME = 'Home',
  EASY = 'Easy',
  FAST = 'Fast',
  POWERFUL = 'Powerful',
  SECURE = 'Secure',
  DISPLAY = 'Display',
  KEYBOARD = 'Keyboard',
  GRAPHICS = 'Graphics',
  GAME = 'Game',
  PERIPHERALS = 'Peripherals',
  PERFORMANCE = 'Performance',
  APPS = 'Apps',
  CHROMEOS = 'ChromeOS',

  // New in 2024 Cycle 1 refresh
  // CBX:
  GOOGLE_AI = 'GoogleAI',
  EASY_TO_USE = 'EasyToUse',
  // CB (Shared with CBX: Performance, Apps):
  GOOGLE_BUILT_IN = 'GoogleBuiltIn',
}

/**
 * The name for the details pages in the Highlights app.
 */
export enum DetailsPage {
  // 2023 CBX first released:
  ADOBE = 'Adobe',
  BATTERY = 'Battery',
  COMPARISON = 'Comparison',
  DISPLAY_ENTERTAINMENT = 'DisplayEntertainment',
  DISPLAY_PERFORMANCE = 'DisplayPerformance',
  ENTERTAINMENT_APPS = 'EntertainmentApps',
  GOOGLE_APPS = 'GoogleApps',
  LUMAFUSION = 'LumaFusion',
  MESSAGING = 'Messaging',
  MOBILE_GAMING = 'MobileGaming',
  MS_365_APPS = 'MS365Apps',
  MS_OFFICE = 'MSOffice',
  NEARBY_SHARE = 'NearbyShare',
  OFFLINE_MODE = 'OfflineMode',
  PC_CONSOLE_GAMING = 'PCConsoleGaming',
  PHOTOS = 'Photos',
  PROCESSOR = 'Processor',
  STORAGE = 'Storage',
  SWITCHING = 'Switching',
  VIDEO_CALL = 'VideoCall',

  // New in 2024 Cycle 1 refresh
  // CBX:
  BUILT_IN_SECURITY = 'BuiltInSecurity',
  WEBCAM = 'Webcam',
  GAME_DASH_BOARD = 'GameDashboard',
  GEMINI_FOR_ALL = 'GeminiForAll',
  HELP_ME_WRITE = 'HelpMeWrite',
  GEMINI_FOR_WORK_SPACE = 'GeminiForWorkSpace',
  AI_BACKGROUND = 'AIBackground',
  AI_PREMIUM_PLAN = 'AIPremiumPlan',

  // CB, note that detail page for generic was not recorded before 2024 C1:
  FAST_BOOT = 'FastBoot',
  AUTO_UPDATE = 'AutoUpdate',
  EASY_SETUP = 'EasySetup',
  LAUNCHER_SEARCH = 'LauncherSearch',
  GOOGLE_TOOLS_BUILT_IN = 'GoogleToolsBuiltIn',
  TITAN_C2 = 'TitanC2',
  CREATIVITY = 'Creativity',
  ENTERTAINMENT = 'Entertainment',
  PRODUCTIVITY = 'Productivity',
  PLAY_STORE = 'PlayStore',

  // Enum shared between CB & CBX are: BATTERY, GOOGLE_APPS, NEARBY_SHARE,
  // MESSAGING, BUILT_IN_SECURITY,MS_365_APPS, SWITCHING, COMPARISON

  // New in 2024 Cycle 2 refresh
  // CBX:
  HELP_ME_READ = 'HelpMeRead',
  LIVE_TRANSLATE = 'LiveTranslate',

  // New in 2025 Refresh
  // CB Generic:
  QUICK_INSERT = 'QuickInsert',
  GEMINI_PWA = 'GeminiPWA',
  SELECT_TO_SEARCH = 'SelectToSearch',
  GEMINI_ADVANCE = 'GeminiAdvance',
  DRIVE_INTEGRATION = 'DriveIntegration',
  BETTER_TOGETHER = 'BetterTogether',
  PHONE_HUB = 'PhoneHub',
  WELCOM_RECAP = 'WelcomeRecap',
  // CBX:
  FILE_SYNC = 'FileSync',
  MAGIC_EDITOR = 'MagicEditor',
  // Enum shared between CB & CBX are: GEMINI_PWA, SELECT_TO_SEARCH,
  // QUICK_INSERT, GEMINI_ADVANCE, BETTER_TOGETHER, and PHONE_HUB
}

/**
 * The buttons in each pillar page of the Highlights app.
 */
export enum PillarButton {
  NEXT = 'Next',
  PREVIOUS = 'Previous',
}

/**
 * Errors in the Highlights app.
 *
 * This is used by histogram: DemoMode.Highlights.Error
 *
 * These values are persisted to logs, so entries should not be renumbered and
 * numeric values should never be reused.
 */
enum DemoModeHighlightsError {
  ATTRACTION_LOOP_TIMESTAMP_INVALID = 0,
  PAGE_VIEW_DURATION_INVALID = 1,
  DETAILS_PAGE_VIEW_DURATION_INVALID = 2,
}

/**
 * A map between the Page in this js file and DemoModeHighlightsAction enum in
 * the UMA enums.xml.
 */
const FirstInteractionActionMap: Map<string, number> = new Map([
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
  [Page.GOOGLE_AI, 13],
  [Page.EASY_TO_USE, 14],
  [Page.GOOGLE_BUILT_IN, 15],

  ['MAX_VALUE', 15],
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
  firstInteractionRecorded: boolean = false;

  // Record the action that the user breaks the current Attract Loop.
  recordAttractLoopBreak() {
    chrome.metricsPrivateIndividualApis.recordUserAction(
        'DemoMode_AttractLoop_Break');
  }

  /**
   * Record the timestamp (i.e. milliseconds from the beginning of the Attract
   * Loop video) at which the user broke the Attract Loop.
   */
  recordAttractLoopBreakTimestamp(timestampInMilliseconds: number) {
    if (isNaN(timestampInMilliseconds)) {
      this.recordError_(
          DemoModeHighlightsError.ATTRACTION_LOOP_TIMESTAMP_INVALID);
      return;
    }
    chrome.metricsPrivateIndividualApis.recordMediumTime(
        'DemoMode.AttractLoop.Timestamp',
        timestampInMilliseconds,
    );
  }

  /**
   * Record the first action of current user.
   */
  recordFirstInteraction(action: Page) {
    if (!this.firstInteractionRecorded) {
      chrome.metricsPrivateIndividualApis.recordEnumerationValue(
          'DemoMode.Highlights.FirstInteraction',
          FirstInteractionActionMap.get(action)!,
          FirstInteractionActionMap.get('MAX_VALUE')!);
      this.firstInteractionRecorded = true;
    }
  }

  /**
   * Record the button clicks in home page of the current user.
   */
  recordHomePageButtonClick(page: Page) {
    chrome.metricsPrivateIndividualApis.recordUserAction(
        'DemoMode_Highlights_HomePage_Click_' + page + 'Button');
    this.recordFirstInteraction(page);
  }

  /**
   * Record the button clicks in home page of the current user.
   */
  recordNavbarButtonClick(page: Page) {
    chrome.metricsPrivateIndividualApis.recordUserAction(
        'DemoMode_Highlights_Navbar_Click_' + page + 'Button');
    this.recordFirstInteraction(page);
  }

  /**
   * Record the button click in pillar pages of the current user.
   */
  recordPillarPageButtonClick(pillarButton: PillarButton) {
    chrome.metricsPrivateIndividualApis.recordUserAction(
        'DemoMode_Highlights_PillarPage_Click_' + pillarButton + 'Button');
  }

  /**
   * Record the duration of the user staying on the page.
   */
  recordPageViewDuration(page: Page, durationInMilliseconds: number) {
    if (!durationInMilliseconds) {
      this.recordError_(DemoModeHighlightsError.PAGE_VIEW_DURATION_INVALID);
      return;
    }
    chrome.metricsPrivateIndividualApis.recordMediumTime(
        'DemoMode.Highlights.PageStayDuration.' + page + 'Page',
        durationInMilliseconds);
  }

  /**
   * Record the details page clicked by the current user
   */
  recordDetailsPageClicked(detailsPage: DetailsPage) {
    chrome.metricsPrivateIndividualApis.recordUserAction(
        'DemoMode_Highlights_DetailsPage_Clicked_' + detailsPage + 'Button');
  }

  /**
   * Record the duration of the user staying on a details page
   */
  recordDetailsPageViewDuration(
      detailsPage: DetailsPage, durationInMilliseconds?: number) {
    if (!durationInMilliseconds) {
      this.recordError_(
          DemoModeHighlightsError.DETAILS_PAGE_VIEW_DURATION_INVALID);
      return;
    }
    chrome.metricsPrivateIndividualApis.recordMediumTime(
        'DemoMode.Highlights.DetailsPageStayDuration.' + detailsPage + 'Page',
        durationInMilliseconds);
  }

  /**
   * Record error in highlight app.
   */
  private recordError_(error: DemoModeHighlightsError) {
    const maxValue = Object.keys(DemoModeHighlightsError).length;
    chrome.metricsPrivateIndividualApis.recordEnumerationValue(
        'DemoMode.Highlights.Error', error, maxValue);
  }
}



export const metricsService = new DemoMetricsService();
