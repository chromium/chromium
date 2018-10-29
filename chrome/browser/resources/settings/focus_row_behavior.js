// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @implements {cr.ui.FocusRowDelegate} */
class FocusRowDelegate {
  /** @param {{lastFocused: Object}} listItem */
  constructor(listItem) {
    /** @private */
    this.listItem_ = listItem;
  }

  /**
   * This function gets called when the [focus-row-control] element receives
   * the focus event.
   * @override
   * @param {!cr.ui.FocusRow} row
   * @param {!Event} e
   */
  onFocus(row, e) {
    this.listItem_.lastFocused = e.path[0];
    this.listItem_.tabIndex = -1;
  }

  /**
   * @override
   * @param {!cr.ui.FocusRow} row The row that detected a keydown.
   * @param {!Event} e
   * @return {boolean} Whether the event was handled.
   */
  onKeydown(row, e) {
    // Prevent iron-list from changing the focus on enter.
    if (e.key == 'Enter')
      e.stopPropagation();

    return false;
  }
}

/** @extends {cr.ui.FocusRow} */
class VirtualFocusRow extends cr.ui.FocusRow {
  /**
   * @param {!Element} root
   * @param {cr.ui.FocusRowDelegate} delegate
   */
  constructor(root, delegate) {
    super(root, /* boundary */ null, delegate);
  }
}

/**
 * Any element that is being used as an iron-list row item can extend this
 * behavior, which encapsulates focus controls of mouse and keyboards.
 * To use this behavior:
 *    - The parent element should pass a "last-focused" attribute double-bound
 *      to the row items, to track the last-focused element across rows.
 *    - There must be a container in the extending element with the
 *      [focus-row-container] attribute that contains all focusable controls.
 *    - On each of the focusable controls, there must be a [focus-row-control]
 *      attribute, and a [focus-type=] attribute unique for each control.
 *
 * @polymerBehavior
 */
const FocusRowBehavior = {
  properties: {
    /** @private {VirtualFocusRow} */
    row_: Object,

    /** @private {boolean} */
    mouseFocused_: Boolean,

    /** @type {Element} */
    lastFocused: {
      type: Object,
      notify: true,
    },

    /**
     * This is different from tabIndex, since the template only does a one-way
     * binding on both attributes, and the behavior actually make use of this
     * fact. For example, when a control within a row is focused, it will have
     * tabIndex = -1 and ironListTabIndex = 0.
     * @type {number}
     */
    ironListTabIndex: {
      type: Number,
      observer: 'ironListTabIndexChanged_',
    },
  },

  /** @override */
  attached: function() {
    this.classList.add('no-outline');

    Polymer.RenderStatus.afterNextRender(this, function() {
      const rowContainer = this.root.querySelector('[focus-row-container]');
      assert(!!rowContainer);
      this.row_ = new VirtualFocusRow(rowContainer, new FocusRowDelegate(this));
      this.ironListTabIndexChanged_();
      this.addItems_();

      // Adding listeners asynchronously to reduce blocking time, since this
      // behavior will be used by items in potentially long lists.
      this.listen(this, 'focus', 'onFocus_');
      this.listen(this, 'dom-change', 'addItems_');
      this.listen(this, 'mousedown', 'onMouseDown_');
      this.listen(this, 'blur', 'onBlur_');
    });
  },

  /** @override */
  detached: function() {
    this.unlisten(this, 'focus', 'onFocus_');
    this.unlisten(this, 'dom-change', 'addItems_');
    this.unlisten(this, 'mousedown', 'onMouseDown_');
    this.unlisten(this, 'blur', 'onBlur_');
    if (this.row_)
      this.row_.destroy();
  },

  /** @private */
  addItems_: function() {
    if (this.row_) {
      this.row_.destroy();

      const controls = this.root.querySelectorAll('[focus-row-control]');

      controls.forEach(control => {
        this.row_.addItem(
            control.getAttribute('focus-type'),
            /** @type {!HTMLElement} */
            (cr.ui.FocusRow.getFocusableElement(control)));
      });
    }
  },

  /**
   * This function gets called when the row itself receives the focus event.
   * @private
   */
  onFocus_: function() {
    if (this.mouseFocused_) {
      this.mouseFocused_ = false;  // Consume and reset flag.
      return;
    }

    if (this.lastFocused) {
      this.row_.getEquivalentElement(this.lastFocused).focus();
    } else {
      const firstFocusable = assert(this.row_.getFirstFocusable());
      firstFocusable.focus();
    }

    this.tabIndex = -1;
  },

  /** @private */
  ironListTabIndexChanged_: function() {
    if (this.row_)
      this.row_.makeActive(this.ironListTabIndex == 0);
  },

  /** @private */
  onMouseDown_: function() {
    this.mouseFocused_ = true;  // Set flag to not do any control-focusing.
  },

  /** @private */
  onBlur_: function() {
    this.mouseFocused_ = false;  // Reset flag since it's not active anymore.
  }
};
