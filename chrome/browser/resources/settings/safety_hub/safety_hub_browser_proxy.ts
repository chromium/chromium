// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used by the "Site Settings" to interact with
 * the permission-related updates of the browser.
 */

// clang-format off
import {sendWithPromise} from 'chrome://resources/js/cr.js';

import type {ContentSettingsTypes} from '../site_settings/constants.js';
// clang-format on

/**
 * Constants used in safety hub C++ to JS communication.
 * Their values need be kept in sync with their counterparts in
 * chrome/browser/ui/webui/settings/safety_hub_handler.h and
 * chrome/browser/ui/webui/settings/safety_hub_handler.cc
 */
export enum SafetyHubEvent {
  UNUSED_PERMISSIONS_MAYBE_CHANGED =
      'unused-permission-review-list-maybe-changed',
  NOTIFICATION_PERMISSIONS_MAYBE_CHANGED =
      'notification-permission-review-list-maybe-changed',
  EXTENSIONS_CHANGED = 'extensions-review-list-maybe-changed',
}

// The notification permission information passed from safety_hub_handler.cc.
export interface NotificationPermission {
  origin: string;
  notificationInfoString: string;
}

// The unused site permission information passed from safety_hub_handler.cc.
export interface UnusedSitePermissions {
  origin: string;
  permissions: ContentSettingsTypes[];
  expiration: string;
}

// The information for top cards in Safety Hub page.
export interface CardInfo {
  header: string;
  subheader: string;
  state: CardState;
}

/**
 * A Safety Hub card has 4 different states as represented below. Depending on
 * the card state, the card will be updated.
 * Should be kept in sync with the corresponding enum in
 * chrome/browser/ui/safety_hub/safety_hub_constants.h.
 */
export enum CardState {
  WARNING,
  WEAK,
  INFO,
  SAFE,
}

// The information for the entry point of the Safety Hub on Privacy and Security
// page.
export interface EntryPointInfo {
  hasRecommendations: boolean;
  header: string;
  subheader: string;
}

export interface SafetyHubBrowserProxy {
  /**
   * Mark revoked permissions of unused sites as reviewed by the user so they
   * will not be shown again.
   */
  acknowledgeRevokedUnusedSitePermissionsList(): void;

  /**
   * Allow permissions again for an unused site where permissions were
   * auto-revoked. The origin will not appear again for the user to review and
   * permissions will not be auto-revoked for this origin in the future.
   */
  allowPermissionsAgainForUnusedSite(origin: string): void;

  /**
   * Gets the unused origins along with the permissions they have been granted.
   */
  getRevokedUnusedSitePermissionsList(): Promise<UnusedSitePermissions[]>;

  /**
   * Reverse the changes made by |acknowledgeRevokedUnusedSitePermissionsList|.
   * The list of sites will be presented again to the user for review.
   */
  undoAcknowledgeRevokedUnusedSitePermissionsList(
      unusedSitePermissionsList: UnusedSitePermissions[]): void;

  /**
   * Reverse the changes made by |allowPermissionsAgainForUnusedSite|. This will
   * revoke the origin's permissions, re-enable auto-revocation for this origin,
   * and the entry will be visible again in the UI.
   */
  undoAllowPermissionsAgainForUnusedSite(unusedSitePermissions:
                                             UnusedSitePermissions): void;

  /** Gets the site list that send a lot of notifications. */
  getNotificationPermissionReview(): Promise<NotificationPermission[]>;

  /** Blocks the notification permission for all origins in the list. */
  blockNotificationPermissionForOrigins(origins: string[]): void;

  /** Allows the notification permission for all origins in the list */
  allowNotificationPermissionForOrigins(origins: string[]): void;

  /** Adds the origins to blocklist for the notification permissions feature. */
  ignoreNotificationPermissionForOrigins(origins: string[]): void;

  /**
   * Removes the origins from the blocklist for the notification permissions
   * feature.
   */
  undoIgnoreNotificationPermissionForOrigins(origins: string[]): void;

  /** Resets the notification permission for the origins. */
  resetNotificationPermissionForOrigins(origin: string[]): void;

  /**
   * When Safety Hub is visited, the active three-dot menu notification is
   * dismissed, if there is any.
   */
  dismissActiveMenuNotification(): void;

  /** Gets data for the password top card. */
  getPasswordCardData(): Promise<CardInfo>;

  /** Gets data for the Safe Browsing top card. */
  getSafeBrowsingCardData(): Promise<CardInfo>;

  /** Gets data for the version top card. */
  getVersionCardData(): Promise<CardInfo>;

  /** Get the number of extensions that should be reviewed by the user. */
  getNumberOfExtensionsThatNeedReview(): Promise<number>;

  /** Get the subheader for Safety Hub entry point in settings. */
  getSafetyHubEntryPointData(): Promise<EntryPointInfo>;

  /* Record a visit to the Safety Hub page. */
  recordSafetyHubPageVisit(): void;

  /* Record an interaction on the Safety Hub page. */
  recordSafetyHubInteraction(): void;
}

export class SafetyHubBrowserProxyImpl implements SafetyHubBrowserProxy {
  acknowledgeRevokedUnusedSitePermissionsList() {
    chrome.send('acknowledgeRevokedUnusedSitePermissionsList');
  }

  allowPermissionsAgainForUnusedSite(origin: string) {
    chrome.send('allowPermissionsAgainForUnusedSite', [origin]);
  }

  getRevokedUnusedSitePermissionsList() {
    return sendWithPromise('getRevokedUnusedSitePermissionsList');
  }

  undoAcknowledgeRevokedUnusedSitePermissionsList(unusedSitePermissionsList:
                                                      UnusedSitePermissions[]) {
    chrome.send(
        'undoAcknowledgeRevokedUnusedSitePermissionsList',
        [unusedSitePermissionsList]);
  }

  undoAllowPermissionsAgainForUnusedSite(unusedSitePermissions:
                                             UnusedSitePermissions) {
    chrome.send(
        'undoAllowPermissionsAgainForUnusedSite', [unusedSitePermissions]);
  }

  getNotificationPermissionReview() {
    return sendWithPromise('getNotificationPermissionReview');
  }

  blockNotificationPermissionForOrigins(origins: string[]) {
    chrome.send('blockNotificationPermissionForOrigins', [origins]);
  }

  allowNotificationPermissionForOrigins(origins: string[]) {
    chrome.send('allowNotificationPermissionForOrigins', [origins]);
  }

  ignoreNotificationPermissionForOrigins(origins: string[]) {
    chrome.send('ignoreNotificationPermissionReviewForOrigins', [origins]);
  }

  undoIgnoreNotificationPermissionForOrigins(origins: string[]) {
    chrome.send('undoIgnoreNotificationPermissionReviewForOrigins', [origins]);
  }

  resetNotificationPermissionForOrigins(origins: string[]) {
    chrome.send('resetNotificationPermissionForOrigins', [origins]);
  }

  dismissActiveMenuNotification() {
    chrome.send('dismissActiveMenuNotification');
  }

  getPasswordCardData() {
    return sendWithPromise('getPasswordCardData');
  }

  getSafeBrowsingCardData() {
    return sendWithPromise('getSafeBrowsingCardData');
  }

  getVersionCardData() {
    return sendWithPromise('getVersionCardData');
  }

  getNumberOfExtensionsThatNeedReview() {
    return sendWithPromise('getNumberOfExtensionsThatNeedReview');
  }

  getSafetyHubEntryPointData() {
    return sendWithPromise('getSafetyHubEntryPointData');
  }

  recordSafetyHubPageVisit() {
    return sendWithPromise('recordSafetyHubPageVisit');
  }

  recordSafetyHubInteraction() {
    return sendWithPromise('recordSafetyHubInteraction');
  }

  static getInstance(): SafetyHubBrowserProxy {
    return instance || (instance = new SafetyHubBrowserProxyImpl());
  }

  static setInstance(obj: SafetyHubBrowserProxy) {
    instance = obj;
  }
}

let instance: SafetyHubBrowserProxy|null = null;
