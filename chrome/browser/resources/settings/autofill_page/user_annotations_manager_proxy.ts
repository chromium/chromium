// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

type UserAnnotationsEntry = chrome.autofillPrivate.UserAnnotationsEntry;

/**
 * This interface defines the autofill API wrapper that combines user
 * annotations related methods.
 */
export interface UserAnnotationsManagerProxy {
  /**
   * Returns user annotations entries.
   */
  getEntries(): Promise<UserAnnotationsEntry[]>;

  /**
   * Returns true if there are any user annotation entries.
   */
  hasEntries(): Promise<boolean>;

  /**
   * Delete the user annotation entry by its id.
   */
  deleteEntry(entryId: number): void;

  /**
   * Deletes all user annotation entries.
   */
  deleteAllEntries(): void;

  /**
   * Returns whether the user is eligible for autofill prediction improvements.
   */
  isUserEligible(): Promise<boolean>;

  /*
   * Notifies user education that the user used the pref.
   */
  predictionImprovementsIphFeatureUsed(): void;

  /**
   * Starts the bootstrapping process for user annotations.
   *
   * This method is called when the user enables the prediction improvements. It
   * will only start the bootstrapping process if the user is eligible and
   * doesn't have existing user annotation entries. *
   * @return `true` when the bootstrapping process resulted in user annotations
   *     being added, `false` otherwise.
   */
  triggerBootstrapping(): Promise<boolean>;
}

export class UserAnnotationsManagerProxyImpl implements
    UserAnnotationsManagerProxy {
  getEntries(): Promise<UserAnnotationsEntry[]> {
    return chrome.autofillPrivate.getUserAnnotationsEntries();
  }

  hasEntries(): Promise<boolean> {
    return chrome.autofillPrivate.hasUserAnnotationsEntries();
  }

  deleteEntry(entryId: number): void {
    chrome.autofillPrivate.deleteUserAnnotationsEntry(entryId);
  }

  deleteAllEntries(): void {
    chrome.autofillPrivate.deleteAllUserAnnotationsEntries();
  }

  isUserEligible(): Promise<boolean> {
    return chrome.autofillPrivate.isUserEligibleForAutofillImprovements();
  }

  predictionImprovementsIphFeatureUsed(): void {
    chrome.autofillPrivate.predictionImprovementsIphFeatureUsed();
  }

  triggerBootstrapping(): Promise<boolean> {
    return chrome.autofillPrivate.triggerAnnotationsBootstrapping();
  }

  static getInstance(): UserAnnotationsManagerProxy {
    return instance || (instance = new UserAnnotationsManagerProxyImpl());
  }

  static setInstance(obj: UserAnnotationsManagerProxy): void {
    instance = obj;
  }
}

let instance: UserAnnotationsManagerProxy|null = null;
