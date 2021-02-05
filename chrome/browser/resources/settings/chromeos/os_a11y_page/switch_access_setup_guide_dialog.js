// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This dialog walks a user through the flow of setting up Switch
 * Access.
 */

Polymer({
  is: 'settings-switch-access-setup-guide-dialog',

  behaviors: [
    I18nBehavior,
  ],

  properties: {},

  /** @private */
  onExitClick_() {
    this.$.switchAccessSetupGuideDialog.close();
  }
});
