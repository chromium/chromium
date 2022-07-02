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

  // Returns either the passed 'title-label-key', or uses the 'title-key'.
  getAriaLabel(locale, titleLabelKey, titleKey) {
    assert(this.titleLabelKey || this.titleKey,
           'OOBE Loading dialog requires a title or a label for a11y!');
    return (titleLabelKey) ? this.i18n(titleLabelKey) : this.i18n(titleKey);
  },

  cancel() {
    assert(this.canCancel);
    this.fire('cancel-loading');
  },
});
