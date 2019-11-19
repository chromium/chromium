// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Offline message screen implementation.
 */

login.createScreen('TPMErrorMessageScreen', 'tpm-error-message', function() {
  return {
    EXTERNAL_API: ['show'],

    /** @override */
    decorate: function() {
      $('tpm-restart-button').addEventListener('click', function(e) {
        chrome.send('rebootSystem');
      });
    },

    /**
     * Show TPM screen.
     */
    show: function() {
      Oobe.showScreen({id: SCREEN_TPM_ERROR});
    }
  };
});
