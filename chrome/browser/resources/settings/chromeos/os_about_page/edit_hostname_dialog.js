// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'edit-hostname-dialog' is a component allowing the
 * user to edit the device hostname.
 */
Polymer({
  is: 'edit-hostname-dialog',

  /** @private */
  onCancelTap_() {
    this.$.dialog.close();
  },

  /** @private */
  onDoneTap_() {
    this.$.dialog.close();
  },
});
