// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'oobe-help-dialog',

  behaviors: [OobeI18nBehavior],

  properties: {
    /* The ID of the localized string to be used as title text when no "title"
     * slot elements are specified.
     */
    titleKey: {
      type: String,
    },
    /* The ID of the localized string to be used as the content when no
     * "content" slot elements are specified.
     */
    contentKey: {
      type: String,
    },
  },

  ready: function() {},

  /* Shows the help dialog and changes the focus to the close button. */
  showDialog: function() {
    chrome.send('enableShelfButtons', [false]);
    this.$.helpDialog.showModal();
    this.$.closeButton.focus();
  },

  hideDialog: function() {
    this.$.helpDialog.close();
  },

  onClose_() {
    chrome.send('enableShelfButtons', [true]);
  },

});
