// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Discover OOBE/Login screen.
 */

login.createScreen('DiscoverScreen', 'discover', function() {
  return {
    /**
     * Returns the control which should receive initial focus.
     */
    get defaultControl() {
      return $('discover-impl');
    },

    /**
     * Event handler that is invoked just before the screen is shown.
     * @param {object} data Screen init payload.
     */
    onBeforeShow: function(data) {
      $('discover-impl').addEventListener('discover-done', function() {
        chrome.send('login.DiscoverScreen.userActed', ['finished']);
      });
    },

    /**
     * This is called after resources are updated.
     */
    updateLocalizedContent: function() {
      $('discover-impl').updateLocalizedContent();
    },
  };
});
