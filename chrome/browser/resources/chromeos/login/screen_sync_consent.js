// Copyright 2007 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview First user log in Sync Consent screen implementation.
 */

login.createScreen('SyncConsentScreen', 'sync-consent', function() {
  return {
    EXTERNAL_API: ['onUserSyncPrefsKnown', 'setThrobberVisible'],

    /**
     * Returns the control which should receive initial focus.
     */
    get defaultControl() {
      return $('sync-consent-impl');
    },

    /**
     * Event handler that is invoked just before the screen is hidden.
     */
    onBeforeHide: function() {
      this.setThrobberVisible(false /*visible*/);
    },

    /**
     * This is called after resources are updated.
     */
    updateLocalizedContent: function() {
      $('sync-consent-impl').updateLocalizedContent();
    },

    /**
     * This is called once user sync preferences are known.
     * @param {boolean} sync_everything Whether sync_everything is enabled.
     * @param {boolean} is_managed Whether sync preferences are managed.
     */
    onUserSyncPrefsKnown: function(sync_everything, is_managed) {
      $('sync-consent-impl').onUserSyncPrefsKnown(sync_everything, is_managed);
    },

    /**
     * This is called to show/hide the loading UI.
     * @param {boolean} visible whether to show loading UI.
     */
    setThrobberVisible: function(visible) {
      $('sync-loading').hidden = !visible;
      $('sync-consent-impl').hidden = visible;
      if (visible) {
        $('sync-loading').focus();
      } else {
        $('sync-consent-impl').focus();
      }
    },
  };
});
