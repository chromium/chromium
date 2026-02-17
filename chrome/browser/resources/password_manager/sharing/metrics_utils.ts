// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Utility functions and enums for password sharing metrics.
 */

/**
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 */
export enum PasswordSharingActions {
  // LINT.IfChange
  PASSWORD_DETAILS_SHARE_BUTTON_CLICKED = 0,
  NOT_FAMILY_MEMBER_GOT_IT_CLICKED = 1,
  NOT_FAMILY_MEMBER_CREATE_FAMILY_CLICKED = 2,
  NO_OTHER_FAMILY_MEMBERS_GOT_IT_CLICKED = 3,
  NO_OTHER_FAMILY_MEMBERS_INVITE_LINK_CLICKED = 4,
  ERROR_DIALOG_TRY_AGAIN_CLICKED = 5,
  ERROR_DIALOG_CANCELED = 6,
  FAMILY_PICKER_SHARE_WITH_ONE_MEMBER = 7,
  FAMILY_PICKER_SHARE_WITH_MULTIPLE_MEMBERS = 8,
  FAMILY_PICKER_CANCELED = 9,
  FAMILY_PICKER_VIEW_FAMILY_CLICKED = 10,
  CONFIRMATION_DIALOG_SHARING_CANCELED = 11,
  /*  Deprecated in M122 (b/317798360).
  CONFIRMATION_DIALOG_LEARN_MORE_CLICKED = 12,
  */
  CONFIRMATION_DIALOG_CHANGE_PASSWORD_CLICKED = 13,
  DIALOG_HEADER_HELP_ICON_BUTTON_CLICKED = 14,
  FAMILY_PICKER_OPENED = 15,
  // Must be last.
  COUNT = 16,
  // LINT.ThenChange(//tools/metrics/histograms/metadata/password/enums.xml)
}

/**
 * This should be kept in sync with the enum in
 * components/password_manager/core/browser/password_manager_metrics_util.h.
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 * @enum {number}
 */
export enum MoveToAccountStoreTrigger {
  // LINT.IfChange
  SUCCESSFUL_LOGIN_WITH_PROFILE_STORE_PASSWORD = 0,
  EXPLICITLY_TRIGGERED_IN_SETTINGS = 1,
  EXPLICITLY_TRIGGERED_FOR_MULTIPLE_PASSWORDS_IN_SETTINGS = 2,
  USER_OPTED_IN_AFTER_SAVING_LOCALLY = 3,
  EXPLICITLY_TRIGGERED_FOR_SINGLE_PASSWORD_IN_DETAILS_IN_SETTINGS = 4,
  COUNT = 5,
  // LINT.ThenChange(//tools/metrics/histograms/metadata/password/enums.xml)
}

export function recordPasswordSharingInteraction(
    interaction: PasswordSharingActions) {
  chrome.metricsPrivate.recordEnumerationValue(
      'PasswordManager.PasswordSharingDesktop.UserAction', interaction,
      PasswordSharingActions.COUNT);
}

export function recordMoveToAccountStoreOffered(
    trigger: MoveToAccountStoreTrigger) {
  chrome.metricsPrivate.recordEnumerationValue(
      'PasswordManager.AccountStorage.MoveToAccountStoreFlowOffered', trigger,
      MoveToAccountStoreTrigger.COUNT);
}

export function recordMoveToAccountStoreAccepted(
    trigger: MoveToAccountStoreTrigger) {
  chrome.metricsPrivate.recordEnumerationValue(
      'PasswordManager.AccountStorage.MoveToAccountStoreFlowAccepted2', trigger,
      MoveToAccountStoreTrigger.COUNT);
}
