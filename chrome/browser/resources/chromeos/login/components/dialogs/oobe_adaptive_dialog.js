// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'oobe-adaptive-dialog',

  properties: {
    /**
     * If set, prevents lazy instantiation of the dialog.
     */
    noLazy: {
      type: Boolean,
      value: false,
      observer: 'onNoLazyChanged_',
    },
  },

  /**
   * Creates a ResizeObserver and attaches it to the relevant containers
   * to be observed on size changes and scroll position.
   */
  observeScrolling_() {
    if (this.resizeObserver_) {  // Already observing
      return;
    }

    var scrollContainer = this.$$('#scrollContainer');
    var contentContainer = this.$$('#contentContainer');
    if (!scrollContainer || !contentContainer) {
      return;
    }

    this.resizeObserver_ =
        new ResizeObserver(this.applyScrollClassTags_.bind(this));
    this.resizeObserver_.observe(scrollContainer);
    this.resizeObserver_.observe(contentContainer);
  },

  /**
   * Applies the class tags to scrollContainer that control the shadows.
   */
  applyScrollClassTags_() {
    var el = this.$$('#scrollContainer');
    el.classList.toggle('can-scroll', el.clientHeight < el.scrollHeight);
    el.classList.toggle('is-scrolled', el.scrollTop > 0);
    el.classList.toggle(
        'scrolled-to-bottom',
        el.scrollTop + el.clientHeight >= el.scrollHeight);
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
    document.documentElement.setAttribute('new-layout', '');
    this.$$('#lazy').get();
    this.observeScrolling_();
  },

  /**
   * Scroll to the bottom of footer container.
   */
  scrollToBottom() {
    var el = this.$$('#scrollContainer');
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
   * This is called when this dialog is shown.
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
