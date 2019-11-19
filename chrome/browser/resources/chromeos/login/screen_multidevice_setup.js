// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview MultiDevice setup screen for login/OOBE.
 */
login.createScreen('MultiDeviceSetupScreen', 'multidevice-setup', function() {
  return {
    get defaultControl() {
      return $('multidevice-setup-impl');
    },

    /**
     * This is called after resources are updated.
     */
    updateLocalizedContent: function() {
      $('multidevice-setup-impl').updateLocalizedContent();
    },
  };
});
