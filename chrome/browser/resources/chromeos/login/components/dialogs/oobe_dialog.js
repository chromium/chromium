// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'oobe-dialog',

  behaviors: [OobeI18nBehavior, OobeScrollableBehavior],

  properties: {
    /**
     * Controls visibility of the bottom-buttons element.
     */
    hasButtons: {
      type: Boolean,
      value: false,
    },

    /**
     * Hide the box shadow on the top of oobe-bottom
     */
    hideShadow: {
      type: Boolean,
      value: false,
    },

    /**
     * Control visibility of the header container.
     */
    noHeader: {
      type: Boolean,
      value: false,
    },

    /**
     * Removes footer padding.
     */
    noFooterPadding: {
      type: Boolean,
      value: false,
    },

    /**
     * If true footer would be shrunk as much as possible to fit container.
     */
    footerShrinkable: {
      type: Boolean,
      value: false,
    },

    /* The ID of the localized string to be used as title text when no "title"
     * slot elements are specified.
     */
    titleKey: {
      type: String,
    },

    /* The ID of the localized string to be used as subtitle text when no
     * "subtitle" slot elements are specified.
     */
    subtitleKey: {
      type: String,
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

  focus() {
    /* When Network Selection Dialog is shown because user pressed "Back"
       button on EULA screen, display_manager does not inform this dialog that
       it is shown. It ouly focuses this dialog.
       So this emulates show().
       TODO (alemate): fix this once event flow is updated.
    */
    this.show();
  },

  onBeforeShow() {
    document.documentElement.removeAttribute('new-layout');
    this.$$('#lazy').get();
    var footerContainer = this.$$('#footerContainer');
    var scrollContainer = this.$$('#topScrollContainer');
    if (!scrollContainer || !footerContainer) {
      return;
    }
    this.initScrollableObservers(scrollContainer, footerContainer);
  },

  /**
   * Scroll to the bottom of footer container.
   */
  scrollToBottom() {
    var el = this.$$('#topScrollContainer');
    el.scrollTop = el.scrollHeight;
  },

  /**
   * @private
   * Focuses the element. As cr-input uses focusInput() instead of focus() due
   * to bug, we have to handle this separately.
   * TODO(crbug.com/882612): Replace this with focus() in show().
   */
  focusElement_(element) {
    if (element.focusInput) {
      element.focusInput();
      return;
    }
    element.focus();
  },

  /**
   * This is called from oobe_welcome when this dialog is shown.
   */
  show() {
    var focusedElements = this.getElementsByClassName('focus-on-show');
    var focused = false;
    for (var i = 0; i < focusedElements.length; ++i) {
      if (focusedElements[i].hidden)
        continue;

      focused = true;
      Polymer.RenderStatus.afterNextRender(
          this, () => this.focusElement_(focusedElements[i]));
      break;
    }
    if (!focused && focusedElements.length > 0) {
      Polymer.RenderStatus.afterNextRender(
          this, () => this.focusElement_(focusedElements[0]));
    }

    this.fire('show-dialog');
  },

  /** @private */
  onNoLazyChanged_() {
    if (this.noLazy)
      this.$$('#lazy').get();
  }
});
