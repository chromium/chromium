// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';
// clang-format on

/** @interface */
/* #export */ class SwitchAccessSubpageBrowserProxy {
  /**
   * Refresh assignments by requesting SwitchAccessHandler send all readable key
   * names for each action pref via the 'switch-access-assignments-changed'
   * message.
   */
  refreshAssignmentsFromPrefs() {}

  /**
   * Notifies SwitchAccessHandler an assignment dialog has been attached.
   */
  notifySwitchAccessActionAssignmentDialogAttached() {}

  /**
   * Notifies SwitchAccessHandler an assignment dialog is closing.
   */
  notifySwitchAccessActionAssignmentDialogDetached() {}
}

/**
 * @implements {SwitchAccessSubpageBrowserProxy}
 */
/* #export */ class SwitchAccessSubpageBrowserProxyImpl {
  /** @override */
  refreshAssignmentsFromPrefs() {
    chrome.send('refreshAssignmentsFromPrefs');
  }

  /** @override */
  notifySwitchAccessActionAssignmentDialogAttached() {
    chrome.send('notifySwitchAccessActionAssignmentDialogAttached');
  }

  /** @override */
  notifySwitchAccessActionAssignmentDialogDetached() {
    chrome.send('notifySwitchAccessActionAssignmentDialogDetached');
  }
}

// The singleton instance_ is replaced with a test version of this wrapper
// during testing.
cr.addSingletonGetter(SwitchAccessSubpageBrowserProxyImpl);
