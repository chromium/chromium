// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'oobe-adaptive-dialog',

  behaviors: [OobeFocusBehavior],

  properties: {
    /**
     * Hide the box shadow on the top of oobe-bottom
     */
    hideShadow: {
      type: Boolean,
      value: false,
    },

    /**
     * Control visibility of the header container.
     */
    noHeader: {
      type: Boolean,
      value: false,
    },

    /**
     * Removes footer padding.
     */
    noFooterPadding: {
      type: Boolean,
      value: false,
    },

    /**
     * If true footer would be shrunk as much as possible to fit container.
     */
    footerShrinkable: {
      type: Boolean,
      value: false,
    },

    /**
     * If set, prevents lazy instantiation of the dialog.
     */
    noLazy: {
      type: Boolean,
      value: false,
    },
  },

  focus() {
    /* When Network Selection Dialog is shown because user pressed "Back"
       button on EULA screen, display_manager does not inform this dialog that
       it is shown. It ouly focuses this dialog.
       So this emulates show().
       TODO (crbug.com/1159721): fix this once event flow is updated.
    */
    this.show();
  },

  show() {
    this.focusMarkedElement(this);
  },

  onBeforeShow() {
    this.$.dialog.onBeforeShow();
  },

  /**
   * Scroll to the bottom of footer container.
   */
  scrollToBottom() {
    this.$.dialog.scrollToBottom();
  },
});
