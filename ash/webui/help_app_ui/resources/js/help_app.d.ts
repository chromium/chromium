// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Types for go/help-app-externs. Currently this only exists in the chromium
 * repository and is handcrafted. When no help_app JS exists, this file will
 * replace go/help-app-externs, and it can be a regular `.ts` file that both
 * toolchains consume directly. Until then, the internal toolchain builds only
 * off the JS externs and has no knowledge of this file.
 */

/**
 * Each SearchableItem maps to one Data field in the Local Search Service (LSS)
 * Mojo API and represents a single potential search result for in-app search
 * inside the help app. These originate from the untrusted frame and get parsed
 * by the LSS.
 */
declare interface SearchableItem {
  /** The id of this item. */
  id: string;
  /** Title text. Plain localized text. */
  title: string;
  /** Body text. Plain localized text. */
  body: string;
  /** The main category name, e.g. Perks or Help. Plain localized text. */
  mainCategoryName: string;
  /** Any sub category name, e.g. a help topic name. Plain localized text. */
  subcategoryNames?: string[];
  /**
   * Sub headings from the content. Removed from the body text. Plain localized
   * text.
   */
  subheadings?: string[];
  /**
   * The locale that this content is localized in. Empty string means system
   * locale. The format is language[-country] (e.g., en-US) where the language
   * is the 2 or 3 letter code from ISO-639.
   */
  locale: string;
}

/**
 * Each LauncherSearchableItem maps to one Data field in the LSS Mojo API and
 * represents a single potential help app search result in the CrOS launcher.
 * These originate from the untrusted frame and get parsed by the LSS.
 */
declare interface LauncherSearchableItem {
  /** The unique identifier of this item. */
  id: string;
  /** Title text. Plain localized text. */
  title: string;
  /** The main category name, e.g. Perks or Help. Plain localized text. */
  mainCategoryName: string;
  /**
   * List of tags. Each tag is plain localized text. The item will be searchable
   * by these tags.
   */
  tags: string[];
  /**
   * The locale of the tags. This could be different from the locale of the
   * other fields. Empty string means system locale. Same format as the locale
   * field.
   */
  tagLocale: string;
  /**
   * The URL path containing the relevant content, which may or may not contain
   * URL parameters. For example, if the help content is at
   * chrome://help-app/help/sub/3399763/id/1282338#install-user, then the field
   * would be "help/sub/3399763/id/1282338#install-user" for this page.
   */
  urlPathWithParameters: string;
  /**
   * The locale that this content is localized in. Empty string means system
   * locale. The format is language[-country] (e.g., en-US) where the language
   * is the 2 or 3 letter code from ISO-639.
   */
  locale: string;
}

/** A position in a string. For highlighting matches in snippets. */
declare interface Position {
  length: number;
  start: number;
}

/** Response from calling findInSearchIndex. */
declare interface SearchResult {
  id: string;
  /**
   * List of positions corresponding to the title, sorted by start index. Used
   * in snippet.
   */
  titlePositions: Position[]|null;
  /**
   * List of positions corresponding to the body, sorted by start index. Used in
   * snippet.
   */
  bodyPositions: Position[]|null;
  /** Index of the most relevant subheading match. */
  subheadingIndex: number|null;
  /**
   * List of positions corresponding to the most relevant subheading. Used in
   * snippet.
   */
  subheadingPositions: Position[]|null;
}

/** Response from calling findInSearchIndex. */
declare interface FindResponse {
  results?: SearchResult[];
}

/** Device info supplied by the DeviceInfoManager. */
declare interface DeviceInfo {
  /** The board family of the device. e.g. "brya". */
  board: string;
  /** The model of the device. e.g. "taniks". */
  model: string;
  /**
   * The user type of the profile currently running. e.g. "unmanaged".
   * The possible values for this can be found at
   * https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/apps/user_type_filter.cc;l=30;drc=fe09990eb42846949283fa0bbd315f9bc06c54e5.
   */
  userType: string;
  /** If Steam is allowed for the device, regardless of install status. */
  isSteamAllowed: boolean;
}

/**
 * The delegate which exposes open source privileged WebUI functions to HelpApp.
 */
declare interface ClientApiDelegate {
  /**
   * Opens up the built-in chrome feedback dialog.
   * @return Promise which resolves when the request has been acknowledged. If
   *     the dialog could not be opened the promise resolves with an error
   *     message. Resolves with undefined otherwise.
   */
  openFeedbackDialog: () => Promise<string|undefined>;
  /** Opens the on device app controls section of OS settings. */
  showOnDeviceAppControls: () => Promise<void>;
  /** Opens up the parental controls section of OS settings. */
  showParentalControls: () => Promise<void>;
  /** Triggers the call-to-action associated with the given action type id. */
  triggerWelcomeTipCallToAction: (actionTypeId: number) => Promise<void>;
  /** Add or update the content that is stored in the Search Index. */
  addOrUpdateSearchIndex: (data: SearchableItem[]) => Promise<void>;
  /** Clear the content that is stored in the Search Index. */
  clearSearchIndex: () => Promise<void>;
  /** Search the search index for content that matches the given query. */
  findInSearchIndex:
      (query: string, maxResults?: number) => Promise<FindResponse>;
  /** Close the app. Works if the app is open in the background page. */
  closeBackgroundPage: () => undefined;
  /**
   * Replace the content that is stored in the launcher search index.
   *
   * @return Promise that resolves after the update is complete.
   */
  updateLauncherSearchIndex: (data: LauncherSearchableItem[]) => Promise<void>;
  /**
   * Launches the MS365 setup flow (or shows the final screen of the flow if it
   * was already completed).
   */
  launchMicrosoft365Setup: () => Promise<void>;
  /**
   * Request for the release notes notification to be shown to the user. The
   * notification will only be shown if a notification for the help app has not
   * yet been shown in the current milestone.
   */
  maybeShowReleaseNotesNotification: () => Promise<void>;
  /**
   * Gets device info supplied by the DeviceInfoManager. This has to be a Mojo
   * method rather than additional fields in loadTimeData because the info is
   * obtained asynchronously.
   */
  getDeviceInfo: () => Promise<DeviceInfo>;
  /**
   * Opens a valid https:// URL in a new browser tab without getting intercepted
   * by URL capturing logic. If the "HelpAppAutoTriggerInstallDialog" feature
   * flag is enabled, this will automatically trigger the install dialog.
   * Failure to provide a valid https:// URL will cause the Help app renderer
   * process to crash.
   */
  openUrlInBrowserAndTriggerInstallDialog: (url: string) => Promise<void>;
}

/** Launch data that can be read by the app when it first loads. */
declare interface CustomLaunchData {
  delegate: ClientApiDelegate;
}

interface Window {
  customLaunchData: CustomLaunchData;
}
