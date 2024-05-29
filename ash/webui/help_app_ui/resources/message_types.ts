// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Message definitions passed over the HelpApp privileged/unprivileged pipe.
 */

/** Enum for message types. */
export enum Message {
  OPEN_FEEDBACK_DIALOG = 'open-feedback-dialog',
  SHOW_ON_DEVICE_APP_CONTROLS = 'show-on-device-app-controls',
  SHOW_PARENTAL_CONTROLS = 'show-parental-controls',
  TRIGGER_WELCOME_TIP_CALL_TO_ACTION = 'trigger-welcome-tip-call-to-action',
  ADD_OR_UPDATE_SEARCH_INDEX = 'add-or-update-search-index',
  CLEAR_SEARCH_INDEX = 'clear-search-index',
  FIND_IN_SEARCH_INDEX = 'find-in-search-index',
  CLOSE_BACKGROUND_PAGE = 'close-background-page',
  UPDATE_LAUNCHER_SEARCH_INDEX = 'update-launcher-search-index',
  LAUNCH_MICROSOFT_365_SETUP = 'launch-microsoft-365-setup',
  MAYBE_SHOW_RELEASE_NOTES_NOTIFICATION =
      'maybe-show-release-notes-notification',
  GET_DEVICE_INFO = 'get-device-info',
  OPEN_URL_IN_BROWSER_AND_TRIGGER_INSTALL_DIALOG =
      'open-url-in-browser-and-trigger-install-dialog',
}
