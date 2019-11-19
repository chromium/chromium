// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design supervision
 * transition screen.
 */

Polymer({
  is: 'supervision-transition-md',

  behaviors: [I18nBehavior, OobeDialogHostBehavior],

  properties: {
    /**
     * Flag that determines whether supervision is being removed or added.
     */
    isRemovingSupervision_: Boolean,
  },

  setIsRemovingSupervision: function(is_removing_supervision) {
    this.isRemovingSupervision_ = is_removing_supervision;
  },

  /** @override */
  attached: function() {
    cr.addWebUIListener(
        'supervision-transition-failed',
        this.showSupervisionTransitionFailedScreen_.bind(this));
  },

  /** @private */
  getDialogA11yTitle_: function(locale, isRemovingSupervision) {
    return isRemovingSupervision ? this.i18n('removingSupervisionTitle') :
                                   this.i18n('addingSupervisionTitle');
  },

  /** @private */
  showSupervisionTransitionFailedScreen_: function() {
    this.$.supervisionTransitionDialog.hidden = true;
    this.$.supervisionTransitionErrorDialog.hidden = false;
    this.$.supervisionTransitionErrorDialog.show();
    this.$.supervisionTransitionErrorDialog.focus();
  },

  /**
   * On-tap event handler for OK button.
   *
   * @private
   */
  onAcceptAndContinue_: function() {
    chrome.send('finishSupervisionTransition');
  },
});
