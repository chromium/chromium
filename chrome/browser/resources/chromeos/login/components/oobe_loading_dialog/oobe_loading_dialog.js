// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'oobe-loading-dialog',

  behaviors: [OobeI18nBehavior, OobeDialogHostBehavior],

  properties: {
    textKey: {
      type: String,
    },

    isNewLayout_: {
      type: Boolean,
      value() {
        return loadTimeData.valueExists('newLayoutEnabled') &&
            loadTimeData.getBoolean('newLayoutEnabled');
      },
      readOnly: true,
    }
  },

  onBeforeShow() {
    if (this.isNewLayout_) {
      this.$.dialog.onBeforeShow();
      this.$.spinner.setPlay(true);
    } else {
      this.$.dialogOld.onBeforeShow();
    }
  },

  onBeforeHide() {
    if (this.isNewLayout_) {
      this.$.spinner.setPlay(false);
    }
  },
});
