// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'oobe-content-dialog',

  behaviors: [OobeFocusBehavior, OobeScrollableBehavior],

  properties: {
    /**
     * Supports dialog which is shown without buttons.
     */
    noButtons: {
      type: Boolean,
      value: false,
    },

    /**
     * If set, prevents lazy instantiation of the dialog.
     */
    noLazy: {
      type: Boolean,
      value: false,
      observer: 'onNoLazyChanged_',
    },
  },

  onBeforeShow() {
    document.documentElement.setAttribute('new-layout', '');
    this.$$('#lazy').get();
    var contentContainer = this.$$('#contentContainer');
    var scrollContainer = this.$$('#scrollContainer');
    if (!scrollContainer || !contentContainer) {
      return;
    }
    this.initScrollableObservers(scrollContainer, contentContainer);
  },

  focus() {
    /**
     * TODO (crbug.com/1159721): Fix this once event flow of showing step in
     * display_manager is updated.
     */
    this.show();
  },

  show() {
    this.focusMarkedElement(this);
  },

  /** @private */
  onNoLazyChanged_() {
    if (this.noLazy)
      this.$$('#lazy').get();
  }
});
