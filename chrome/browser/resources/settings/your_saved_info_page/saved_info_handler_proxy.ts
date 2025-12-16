// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

/**
 * @fileoverview A helper object used by the "Your saved info" page
 * to interact with the browser to obtain data types counters
 */

export interface PasswordCount {
  passwordCount: number;
  passkeyCount: number;
}

/**
 * Type of HaTS survey, used to gauge user perception on a data management
 * surface.
 */
// LINT.IfChange(DataManagementSurvey)
export enum DataManagementSurvey {
  YOUR_SAVED_INFO = 0,
  PASSWORDS = 1,
  PAYMENTS = 2,
  CONTACT_INFO = 3,
  IDENTITY_DOCS = 4,
  TRAVEL = 5,
}
// LINT.ThenChange(/chrome/browser/ui/webui/settings/saved_info_handler.cc:DataManagementSurvey)

export interface SavedInfoHandlerProxy {
  /**
   * Get the number of passwords and passkeys.
   */
  getPasswordCount(): Promise<PasswordCount>;

  /**
   * Get the number of loyalty cards.
   */
  getLoyaltyCardsCount(): Promise<number|undefined>;

  /**
   * Request launching Happiness Tracking Survey for Your saved info management
   * surface. This will check the user's eligibility to see the survey
   * before displaying it.
   * @param survey category of data management survey for a specific page
   * @param isFromHomePage true if the current page has been visited from main
   * Your saved info page (Home of Transactions)
   */
  requestDataManagementSurvey(
      survey: DataManagementSurvey, isFromHomePage: boolean): void;
}

export class SavedInfoHandlerImpl implements SavedInfoHandlerProxy {
  getPasswordCount() {
    return sendWithPromise('getPasswordCount');
  }

  getLoyaltyCardsCount() {
    return sendWithPromise('getLoyaltyCardsCount');
  }

  requestDataManagementSurvey(
      survey: DataManagementSurvey, isFromHomePage: boolean) {
    chrome.send('requestDataManagementSurvey', [
      survey,
      isFromHomePage,
    ]);
  }

  static getInstance(): SavedInfoHandlerProxy {
    return instance || (instance = new SavedInfoHandlerImpl());
  }

  static setInstance(obj: SavedInfoHandlerProxy) {
    instance = obj;
  }
}

let instance: SavedInfoHandlerProxy|null = null;
