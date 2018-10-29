// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'nux-set-as-default',

  behaviors: [WebUIListenerBehavior],

  /** @private {nux.NuxSetAsDefaultProxy} */
  browserProxy_: null,

  properties: {
    /** @type {nux.stepIndicatorModel} */
    indicatorModel: Object,
  },

  /** @override */
  attached: function() {
    this.browserProxy_ = nux.NuxSetAsDefaultProxyImpl.getInstance();

    this.addWebUIListener(
        'browser-default-state-changed',
        this.onDefaultBrowserChange_.bind(this));
  },

  /** @private */
  onDeclineClick_: function() {
    // TODO(scottchen): Add UMA collection here.

    this.finished_();
  },

  /** @private */
  onSetDefaultClick_: function() {
    this.browserProxy_.setAsDefault();
  },

  /**
   * Automatically navigate to the next onboarding step once default changed.
   * @param {!nux.DefaultBrowserInfo} status
   * @private
   */
  onDefaultBrowserChange_: function(status) {
    // TODO(scottchen): Add UMA collection here.

    if (status.isDefault)
      this.finished_();
  },

  /** @private */
  finished_: function() {
    welcome.navigateToNextStep();
  },
});
