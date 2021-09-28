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

    subtitleKey: {
      type: String,
      value: '',
    },
  },

  onBeforeShow() {
    this.$.spinner.setPlay(true);
  },

  onBeforeHide() {
    this.$.spinner.setPlay(false);
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
});
