// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Message definitions passed over the HelpApp privileged/unprivileged pipe.
 */

/**
 * Enum for message types.
 * @enum {string}
 */
export const Message = {
  OPEN_FEEDBACK_DIALOG: 'open-feedback-dialog',
  SHOW_PARENTAL_CONTROLS: 'show-parental-controls',
  ADD_OR_UPDATE_SEARCH_INDEX: 'add-or-update-search-index',
  CLEAR_SEARCH_INDEX: 'clear-search-index',
  FIND_IN_SEARCH_INDEX: 'find-in-search-index',
  CLOSE_BACKGROUND_PAGE: 'close-background-page',
  UPDATE_LAUNCHER_SEARCH_INDEX: 'update-launcher-search-index',
  MAYBE_SHOW_DISCOVER_NOTIFICATION: 'maybe-show-discover-notification',
  MAYBE_SHOW_RELEASE_NOTES_NOTIFICATION:
      'maybe-show-release-notes-notification',
  GET_DEVICE_INFO: 'get-device-info',
};
