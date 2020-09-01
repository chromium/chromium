// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'os-settings-remove-input-method-dialog' is a dialog for
 * removing an input method.
 */
Polymer({
  is: 'os-settings-remove-input-method-dialog',

  behaviors: [
    I18nBehavior,
  ],

  properties: {
    /** @type {!chrome.languageSettingsPrivate.InputMethod} */
    inputMethod: Object,

    /** @type {!LanguageHelper} */
    languageHelper: Object,
  },

  /** @private */
  getTitle_() {
    return this.i18n('removeInputMethodLabel', this.inputMethod.displayName);
  },

  /** @private */
  onCancelButtonTap_() {
    this.$.dialog.close();
  },

  /** @private */
  onActionButtonTap_() {
    this.languageHelper.removeInputMethod(this.inputMethod.id);
    this.$.dialog.close();
  },
});
