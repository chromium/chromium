// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'os-settings-change-device-language-dialog' is a dialog for
 * changing device language.
 */
Polymer({
  is: 'os-settings-change-device-language-dialog',

  properties: {
    /** @private */
    disableActionButton_: {
      type: Boolean,
      value: true,
    },
  },

  /** @private */
  onCancelButtonTap_() {
    this.$.dialog.close();
  },

  /** @private */
  onActionButtonTap_() {
    this.$.dialog.close();
  },
});
