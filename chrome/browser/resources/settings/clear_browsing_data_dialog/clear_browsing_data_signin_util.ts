// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from '//resources/js/assert.js';
import type {SyncStatus} from '/shared/settings/people_page/sync_browser_proxy.js';
import {SignedInState, StatusAction} from '/shared/settings/people_page/sync_browser_proxy.js';

/**
 * Returns true if the deletion will affect account data. This is only the case
 * if the user is signed in to Chrome with valid credentials. If the user is
 * only signed in to the content area or if they need to re-authenticate (signed
 * in paused state), then account data will not be deleted.
 */
export function canDeleteAccountData(syncStatus: SyncStatus|undefined) {
  return isSignedIn(syncStatus) &&
      syncStatus!.signedInState !== SignedInState.SIGNED_IN_PAUSED &&
      !isSyncPaused(syncStatus!);
}

/** Returns true if the user is signed in to a Google account on Chrome. */
export function isSignedIn(syncStatus: SyncStatus|undefined) {
  if (!syncStatus) {
    return false;
  }

  switch (syncStatus.signedInState) {
    case SignedInState.SIGNED_IN_PAUSED:
    case SignedInState.SIGNED_IN:
    case SignedInState.SYNCING:
      return true;
    case SignedInState.WEB_ONLY_SIGNED_IN:
    case SignedInState.SIGNED_OUT:
      return false;
    default:
      assertNotReached('Invalid SignedInState');
  }
}

function isSyncPaused(syncStatus: SyncStatus): boolean {
  return !!syncStatus.hasError && !syncStatus.hasUnrecoverableError &&
      syncStatus.statusAction === StatusAction.REAUTHENTICATE;
}
