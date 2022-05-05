// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'oobe-loading-dialog',

  behaviors: [OobeI18nBehavior, OobeDialogHostBehavior],

  properties: {
    titleKey: {
      type: String,
    },

    titleLabelKey: {
      type: String,
    },

    subtitleKey: {
      type: String,
      value: '',
    },

    /*
     * If true loading step can be canceled by pressing a cancel button.
     */
    canCancel: {
      type: Boolean,
      value: false,
    },
  },

  onBeforeShow() {
    this.$.spinner.playing = true;
  },

  onBeforeHide() {
    this.$.spinner.playing = false;
  },

  /**
   * Localize subtitle message
   * @private
   * @param {string} locale  i18n locale data
   * @param {string} messageId
   */
  localizeSubtitle_(locale, messageId) {
    return messageId ? this.i18nDynamic(locale, messageId) : '';
  },

  cancel() {
    assert(this.canCancel);
    this.fire('cancel-loading');
  },
});
