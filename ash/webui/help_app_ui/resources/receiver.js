// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * A script for the app inside the iframe. Implements a delegate.
 */
import './sandboxed_load_time_data.js';

import {COLOR_PROVIDER_CHANGED, ColorChangeUpdater} from '//resources/cr_components/color_change_listener/colors_css_updater.js';

import {MessagePipe} from './message_pipe.js';
import {Message} from './message_types.js';

/** A pipe through which we can send messages to the parent frame. */
const parentMessagePipe = new MessagePipe('chrome://help-app', window.parent);

/**
 * A delegate which exposes privileged WebUI functionality to the help
 * app.
 * @type {!helpApp.ClientApiDelegate}
 */
const DELEGATE = {
  async openFeedbackDialog() {
    const response =
        await parentMessagePipe.sendMessage(Message.OPEN_FEEDBACK_DIALOG);
    return /** @type {?string} */ (response['errorMessage']);
  },
  async showParentalControls() {
    await parentMessagePipe.sendMessage(Message.SHOW_PARENTAL_CONTROLS);
  },
  async triggerWelcomeTipCallToAction(actionTypeId) {
    await parentMessagePipe.sendMessage(
        Message.TRIGGER_WELCOME_TIP_CALL_TO_ACTION, actionTypeId);
  },
  /**
   * @override
   * @param {!Array<!helpApp.SearchableItem>} data
   */
  async addOrUpdateSearchIndex(data) {
    await parentMessagePipe.sendMessage(
        Message.ADD_OR_UPDATE_SEARCH_INDEX, data);
  },
  async clearSearchIndex() {
    await parentMessagePipe.sendMessage(Message.CLEAR_SEARCH_INDEX);
  },
  /**
   * @override
   * @param {string} query
   * @param {number=} maxResults Maximum number of search results. Default 50.
   * @return {!Promise<!helpApp.FindResponse>}
   */
  findInSearchIndex(query, maxResults) {
    return /** @type {!Promise<!helpApp.FindResponse>} */ (
        parentMessagePipe.sendMessage(
            Message.FIND_IN_SEARCH_INDEX, {query, maxResults}));
  },
  closeBackgroundPage() {
    parentMessagePipe.sendMessage(Message.CLOSE_BACKGROUND_PAGE);
  },
  /**
   * @override
   * @param {!Array<!helpApp.LauncherSearchableItem>} data
   */
  async updateLauncherSearchIndex(data) {
    await parentMessagePipe.sendMessage(
        Message.UPDATE_LAUNCHER_SEARCH_INDEX, data);
  },
  async launchMicrosoft365Setup() {
    await parentMessagePipe.sendMessage(Message.LAUNCH_MICROSOFT_365_SETUP);
  },
  async maybeShowDiscoverNotification() {
    await parentMessagePipe.sendMessage(
        Message.MAYBE_SHOW_DISCOVER_NOTIFICATION);
  },
  async maybeShowReleaseNotesNotification() {
    await parentMessagePipe.sendMessage(
        Message.MAYBE_SHOW_RELEASE_NOTES_NOTIFICATION);
  },
  getDeviceInfo() {
    return /** @type {!Promise<!helpApp.DeviceInfo>} */ (
        parentMessagePipe.sendMessage(Message.GET_DEVICE_INFO));
  },
  /**
   * @override
   * @param {string} url
   */
  async openUrlInBrowserAndTriggerInstallDialog(url) {
    await parentMessagePipe.sendMessage(
        Message.OPEN_URL_IN_BROWSER_AND_TRIGGER_INSTALL_DIALOG, url);
  },
};

window.customLaunchData = {
  delegate: DELEGATE,
};

window.addEventListener(
    'DOMContentLoaded', /** @suppress {checkTypes} */ function() {
      // Start listening to color change events. These events get picked up by
      // logic in ts_helpers.ts on the google3 side.
      ColorChangeUpdater.forDocument().start();
    });
// Expose functions to bind to color change events to window so they can be
// automatically picked up by installColors(). See ts_helpers.ts in google3.
window['addColorChangeListener'] =
    /** @suppress {checkTypes} */ function(listener) {
      ColorChangeUpdater.forDocument().eventTarget.addEventListener(
          COLOR_PROVIDER_CHANGED, listener);
    };
window['removeColorChangeListener'] =
    /** @suppress {checkTypes} */ function(listener) {
      ColorChangeUpdater.forDocument().eventTarget.removeEventListener(
          COLOR_PROVIDER_CHANGED, listener);
    };

export const TEST_ONLY = {parentMessagePipe};
