// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-crostini-arc-adb-confirmation-dialog' is a component
 * to confirm for enabling or disabling adb sideloading. After the confirmation,
 * reboot will happens.
 */
Polymer({
  is: 'settings-crostini-arc-adb-confirmation-dialog',

  properties: {
    /** An attribute that indicates the action for the confirmation */
    action: {
      type: String,
    },
  },

  /** @override */
  attached: function() {
    this.$.dialog.showModal();
  },

  /** @override */
  isEnabling_: function() {
    return this.action === 'enable';
  },

  /** @override */
  isDisabling_: function() {
    return this.action === 'disable';
  },

  /** @private */
  onCancelTap_: function() {
    this.$.dialog.close();
  },

  /** @private */
  onRestartTap_: function() {
    if (this.isEnabling_()) {
      settings.CrostiniBrowserProxyImpl.getInstance().enableArcAdbSideload();
    } else if (this.isDisabling_()) {
      settings.CrostiniBrowserProxyImpl.getInstance().disableArcAdbSideload();
    } else {
      assertNotReached();
    }
  },
});
