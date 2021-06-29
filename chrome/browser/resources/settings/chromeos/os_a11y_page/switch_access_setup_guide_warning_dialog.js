// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-switch-access-setup-guide-warning-dialog' is a
 * component warning the user that re-running the setup guide will clear their
 * existing switches. By clicking 'Continue', the user acknowledges that.
 */
Polymer({
  is: 'settings-switch-access-setup-guide-warning-dialog',

  /** @override */
  attached() {
    this.$.dialog.showModal();
  },

  /** @private */
  onCancelTap_() {
    this.$.dialog.cancel();
  },

  /** @private */
  onRerunSetupGuideTap_() {
    this.$.dialog.close();
  }
});
