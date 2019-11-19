// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'gaia-password-changed',

  behaviors: [I18nBehavior],

  properties: {
    email: String,

    disabled: {type: Boolean, value: false}
  },

  /** @override */
  ready: function() {
    /**
     * Workaround for
     * https://github.com/PolymerElements/neon-animation/issues/32
     * TODO(dzhioev): Remove when fixed in Polymer.
     */
    var pages = this.$.animatedPages;
    delete pages._squelchNextFinishEvent;
    Object.defineProperty(pages, '_squelchNextFinishEvent', {
      get: function() {
        return false;
      }
    });
  },

  invalidate: function() {
    this.$.oldPasswordInput.isInvalid = true;
  },

  reset: function() {
    this.$.animatedPages.selected = 0;
    this.clearPassword();
    this.$.oldPasswordInput.isInvalid = false;
    this.disabled = false;
    this.$.navigation.closeVisible = true;
    this.$.oldPasswordCard.classList.remove('disabled');
  },


  focus: function() {
    if (this.$.animatedPages.selected == 0)
      this.$.oldPasswordInput.focus();
  },

  /** @private */
  onPasswordSubmitted_: function() {
    if (!this.$.oldPasswordInput.checkValidity())
      return;
    this.$.oldPasswordCard.classList.add('disabled');
    this.disabled = true;
    this.fire('passwordEnter', {password: this.$.oldPasswordInput.value});
  },

  /** @private */
  onForgotPasswordClicked_: function() {
    this.clearPassword();
    this.$.animatedPages.selected += 1;
  },

  /** @private */
  onTryAgainClicked_: function() {
    this.$.oldPasswordInput.isInvalid = false;
    this.$.animatedPages.selected -= 1;
  },

  /** @private */
  onAnimationFinish_: function() {
    this.focus();
  },

  clearPassword: function() {
    this.$.oldPasswordInput.value = '';
  },

  /** @private */
  onProceedClicked_: function() {
    this.disabled = true;
    this.$.navigation.closeVisible = false;
    this.$.animatedPages.selected = 2;
    this.fire('proceedAnyway');
  },

  /** @private */
  onClose_: function() {
    this.fire('cancel');
  }
});
