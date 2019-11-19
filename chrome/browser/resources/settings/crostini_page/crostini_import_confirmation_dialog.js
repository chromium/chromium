// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-crostini-import-confirmation-dialog' is a component
 * warning the user that importing a container overrides the existing container.
 * By clicking 'Continue', the user agrees to start the import.
 */
Polymer({
  is: 'settings-crostini-import-confirmation-dialog',

  /** @override */
  attached: function() {
    this.$.dialog.showModal();
  },

  /** @private */
  onCancelTap_: function() {
    this.$.dialog.close();
  },

  /** @private */
  onContinueTap_: function() {
    settings.CrostiniBrowserProxyImpl.getInstance().importCrostiniContainer();
    this.$.dialog.close();
  },
});
