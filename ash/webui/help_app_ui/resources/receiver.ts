// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * A script for the app inside the iframe. Implements a delegate.
 */

import './sandboxed_load_time_data.js';

import {COLOR_PROVIDER_CHANGED, ColorChangeUpdater} from '//resources/cr_components/color_change_listener/colors_css_updater.js';
import {MessagePipe} from '//system_apps/message_pipe.js';

import {Message} from './message_types.js';

/** A pipe through which we can send messages to the parent frame. */
const parentMessagePipe = new MessagePipe('chrome://help-app', window.parent);

/**
 * A delegate which exposes privileged WebUI functionality to the help
 * app.
 */
const DELEGATE: ClientApiDelegate = {
  async openFeedbackDialog() {
    const response =
        await parentMessagePipe.sendMessage(Message.OPEN_FEEDBACK_DIALOG);
    return response['errorMessage'] as string;
  },
  showOnDeviceAppControls() {
    return parentMessagePipe.sendMessage(Message.SHOW_ON_DEVICE_APP_CONTROLS);
  },
  showParentalControls() {
    return parentMessagePipe.sendMessage(Message.SHOW_PARENTAL_CONTROLS);
  },
  triggerWelcomeTipCallToAction(actionTypeId: number) {
    return parentMessagePipe.sendMessage(
        Message.TRIGGER_WELCOME_TIP_CALL_TO_ACTION, actionTypeId);
  },
  addOrUpdateSearchIndex(data: SearchableItem[]) {
    return parentMessagePipe.sendMessage(
        Message.ADD_OR_UPDATE_SEARCH_INDEX, data);
  },
  clearSearchIndex() {
    return parentMessagePipe.sendMessage(Message.CLEAR_SEARCH_INDEX);
  },
  findInSearchIndex(query: string, maxResults = 50): Promise<FindResponse> {
    return parentMessagePipe.sendMessage(
        Message.FIND_IN_SEARCH_INDEX, {query, maxResults});
  },
  closeBackgroundPage() {
    parentMessagePipe.sendMessage(Message.CLOSE_BACKGROUND_PAGE);
  },
  updateLauncherSearchIndex(data: LauncherSearchableItem[]) {
    return parentMessagePipe.sendMessage(
        Message.UPDATE_LAUNCHER_SEARCH_INDEX, data);
  },
  launchMicrosoft365Setup() {
    return parentMessagePipe.sendMessage(Message.LAUNCH_MICROSOFT_365_SETUP);
  },
  maybeShowReleaseNotesNotification() {
    return parentMessagePipe.sendMessage(
        Message.MAYBE_SHOW_RELEASE_NOTES_NOTIFICATION);
  },
  getDeviceInfo(): Promise<DeviceInfo> {
    return parentMessagePipe.sendMessage(Message.GET_DEVICE_INFO);
  },
  openUrlInBrowserAndTriggerInstallDialog(url: string) {
    return parentMessagePipe.sendMessage(
        Message.OPEN_URL_IN_BROWSER_AND_TRIGGER_INSTALL_DIALOG, url);
  },
};

window.customLaunchData = {
  delegate: DELEGATE,
};

window.addEventListener('DOMContentLoaded', function() {
  // Start listening to color change events. These events get picked up by
  // logic in ts_helpers.ts on the google3 side.
  ColorChangeUpdater.forDocument().start();
});

declare global {
  interface Window {
    addColorChangeListener: (listener: EventListenerOrEventListenerObject|
                             null) => unknown;
    removeColorChangeListener: (listener: EventListenerOrEventListenerObject|
                                null) => unknown;
  }
}

// Expose functions to bind to color change events to window so they can be
// automatically picked up by installColors(). See ts_helpers.ts in google3.
window.addColorChangeListener = function(listener) {
  ColorChangeUpdater.forDocument().eventTarget.addEventListener(
      COLOR_PROVIDER_CHANGED, listener);
};
window.removeColorChangeListener = function(listener) {
  ColorChangeUpdater.forDocument().eventTarget.removeEventListener(
      COLOR_PROVIDER_CHANGED, listener);
};

export const TEST_ONLY = {parentMessagePipe};
