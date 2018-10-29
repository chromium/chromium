// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'oobe-dialog',

  properties: {
    /**
     * Controls visibility of the bottom-buttons element.
     */
    hasButtons: {
      type: Boolean,
      value: false,
    },

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
     * Removes buttons padding.
     */
    noButtonsPadding: {
      type: Boolean,
      value: false,
    },

    /**
     * True when dialog is displayed in full-screen mode.
     */
    fullScreenDialog: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
      observer: 'onfullScreenDialogChanged_',
    },

    android: {
      type: Boolean,
      value: false,
    },
  },

  focus: function() {
    /* When Network Selection Dialog is shown because user pressed "Back"
       button on EULA screen, display_manager does not inform this dialog that
       it is shown. It ouly focuses this dialog.
       So this emulates show().
       TODO (alemate): fix this once event flow is updated.
    */
    this.show();
  },

  onBeforeShow: function() {
    var isOobe = window.hasOwnProperty('Oobe') &&
        window.hasOwnProperty('DISPLAY_TYPE') && Oobe.getInstance() &&
        Oobe.getInstance().displayType == DISPLAY_TYPE.OOBE;
    if (isOobe || document.documentElement.hasAttribute('full-screen-dialog'))
      this.fullScreenDialog = true;
  },

  /**
   * This is called from oobe_welcome when this dialog is shown.
   */
  show: function() {
    var focusedElements = this.getElementsByClassName('focus-on-show');
    var focused = false;
    for (var i = 0; i < focusedElements.length; ++i) {
      if (focusedElements[i].hidden)
        continue;

      focused = true;
      focusedElements[i].focus();
      break;
    }
    if (!focused && focusedElements.length > 0)
      focusedElements[0].focus();

    this.fire('show-dialog');
  },

  onfullScreenDialogChanged_: function() {
    if (this.fullScreenDialog)
      document.documentElement.setAttribute('full-screen-dialog', true);
  },
});
