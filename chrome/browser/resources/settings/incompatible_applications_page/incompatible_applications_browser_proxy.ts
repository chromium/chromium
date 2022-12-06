// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {sendWithPromise} from 'chrome://resources/js/cr.js';
// clang-format on

/**
 * All possible actions to take on an incompatible application.
 *
 * Must be kept in sync with BlacklistMessageType in
 * chrome/browser/win/conflicts/proto/module_list.proto
 */
export enum ActionTypes {
  UNINSTALL = 0,
  MORE_INFO = 1,
  UPGRADE = 2,
}

export interface IncompatibleApplication {
  name: string;
  type: ActionTypes;
  url: string;
}

export interface IncompatibleApplicationsBrowserProxy {
  /**
   * Get the list of incompatible applications.
   */
  requestIncompatibleApplicationsList(): Promise<IncompatibleApplication[]>;

  /**
   * Launches the Apps & Features page that allows uninstalling
   * 'applicationName'.
   */
  startApplicationUninstallation(applicationName: string): void;

  /**
   * Opens the specified URL in a new tab.
   */
  openUrl(url: string): void;

  /**
   * Requests the plural string for the subtitle of the Incompatible
   * Applications subpage.
   */
  getSubtitlePluralString(numApplications: number): Promise<string>;

  /**
   * Requests the plural string for the subtitle of the Incompatible
   * Applications subpage, when the user does not have administrator rights.
   */
  getSubtitleNoAdminRightsPluralString(numApplications: number):
      Promise<string>;

  /**
   * Requests the plural string for the title of the list of Incompatible
   * Applications.
   */
  getListTitlePluralString(numApplications: number): Promise<string>;
}

export class IncompatibleApplicationsBrowserProxyImpl implements
    IncompatibleApplicationsBrowserProxy {
  requestIncompatibleApplicationsList() {
    return sendWithPromise('requestIncompatibleApplicationsList');
  }

  startApplicationUninstallation(applicationName: string) {
    chrome.send('startApplicationUninstallation', [applicationName]);
  }

  openUrl(url: string) {
    window.open(url);
  }

  getSubtitlePluralString(numApplications: number) {
    return sendWithPromise('getSubtitlePluralString', numApplications);
  }

  getSubtitleNoAdminRightsPluralString(numApplications: number) {
    return sendWithPromise(
        'getSubtitleNoAdminRightsPluralString', numApplications);
  }

  getListTitlePluralString(numApplications: number) {
    return sendWithPromise('getListTitlePluralString', numApplications);
  }

  static getInstance(): IncompatibleApplicationsBrowserProxy {
    return instance ||
        (instance = new IncompatibleApplicationsBrowserProxyImpl());
  }

  static setInstance(obj: IncompatibleApplicationsBrowserProxy) {
    instance = obj;
  }
}

let instance: IncompatibleApplicationsBrowserProxy|null = null;
