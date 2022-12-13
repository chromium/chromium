// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: This file is deprecated, and should only be used by legacy code that
// has not yet finished migrating to TypeScript/Polymer class based syntax. New
// code should use focus_row_mixin.ts.

// clang-format off
import {afterNextRender} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assert} from 'chrome://resources/ash/common/assert.js';
import {focusWithoutInk} from './focus_without_ink_js.js';
import {FocusRow, FocusRowDelegate} from './focus_row_js.js';
// clang-format on

  /** @implements {FocusRowDelegate} */
  class FocusRowBehaviorDelegate {
    /**
     * @param {{lastFocused: Object,
     *          overrideCustomEquivalent: boolean,
     *          getCustomEquivalent: (Function|undefined)}} listItem
     */
    constructor(listItem) {
      /** @private */
      this.listItem_ = listItem;
    }

    /**
     * This function gets called when the [focus-row-control] element receives
     * the focus event.
     * @override
     * @param {!FocusRow} row
     * @param {!Event} e
     */
    onFocus(row, e) {
      const element = /** @type {!HTMLElement} */ (e.composedPath()[0]);
      const focusableElement = FocusRow.getFocusableElement(element);
      if (element !== focusableElement) {
        focusableElement.focus();
      }
      this.listItem_.lastFocused = focusableElement;
    }

    /**
     * @override
     * @param {!FocusRow} row The row that detected a keydown.
     * @param {!Event} e
     * @return {boolean} Whether the event was handled.
     */
    onKeydown(row, e) {
      // Prevent iron-list from changing the focus on enter.
      if (e.key === 'Enter') {
        e.stopPropagation();
      }

      return false;
    }

    /** @override */
    getCustomEquivalent(sampleElement) {
      return this.listItem_.overrideCustomEquivalent ?
          this.listItem_.getCustomEquivalent(sampleElement) :
          null;
    }
  }

  /** @extends {FocusRow} */
  class VirtualFocusRow extends FocusRow {
    /**
     * @param {!Element} root
     * @param {FocusRowDelegate} delegate
     */
    constructor(root, delegate) {
      super(root, /* boundary */ null, delegate);
    }

    /** @override */
    getCustomEquivalent(sampleElement) {
      return this.delegate.getCustomEquivalent(sampleElement) ||
          super.getCustomEquivalent(sampleElement);
    }
  }

  /**
   * Any element that is being used as an iron-list row item can extend this
   * behavior, which encapsulates focus controls of mouse and keyboards.
   * To use this behavior:
   *    - The parent element should pass a "last-focused" attribute double-bound
   *      to the row items, to track the last-focused element across rows, and
   *      a "list-blurred" attribute double-bound to the row items, to track
   *      whether the list of row items has been blurred.
   *    - There must be a container in the extending element with the
   *      [focus-row-container] attribute that contains all focusable controls.
   *    - On each of the focusable controls, there must be a [focus-row-control]
   *      attribute, and a [focus-type=] attribute unique for each control.
   *
   * @polymerBehavior
   */
  export const FocusRowBehavior = {
    properties: {
      /** @private {VirtualFocusRow} */
      row_: Object,

      /** @private {boolean} */
      mouseFocused_: Boolean,

      /** Will be updated when |index| is set, unless specified elsewhere. */
      id: {
        type: String,
        reflectToAttribute: true,
      },

      /** For notifying when the row is in focus. */
      isFocused: {
        type: Boolean,
        notify: true,
      },

      /** Should be bound to the index of the item from the iron-list */
      focusRowIndex: {
        type: Number,
        observer: 'focusRowIndexChanged',
      },

      /** @type {HTMLElement} */
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

      listBlurred: {
        type: Boolean,
        notify: true,
      },
    },

    /**
     * Returns an ID based on the index that was passed in.
     * @param {?number} index
     * @return {?string}
     */
    computeId_(index) {
      return index !== undefined ? `frb${index}` : undefined;
    },

    /**
     * Sets |id| if it hasn't been set elsewhere. Also sets |aria-rowindex|.
     * @param {number} newIndex
     * @param {number} oldIndex
     */
    focusRowIndexChanged(newIndex, oldIndex) {
      // focusRowIndex is 0-based where aria-rowindex is 1-based.
      this.setAttribute('aria-rowindex', newIndex + 1);

      // Only set ID if it matches what was previously set. This prevents
      // overriding the ID value if it's set elsewhere.
      if (this.id === this.computeId_(oldIndex)) {
        this.id = this.computeId_(newIndex);
      }
    },

    /** @private {?Element} */
    firstControl_: null,

    /** @private {!Array<!MutationObserver>} */
    controlObservers_: [],

    /** @override */
    attached() {
      this.classList.add('no-outline');

      afterNextRender(this, function() {
        const rowContainer = this.root.querySelector('[focus-row-container]');
        assert(rowContainer);
        this.row_ = new VirtualFocusRow(
            rowContainer, new FocusRowBehaviorDelegate(this));
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
    detached() {
      this.unlisten(this, 'focus', 'onFocus_');
      this.unlisten(this, 'dom-change', 'addItems_');
      this.unlisten(this, 'mousedown', 'onMouseDown_');
      this.unlisten(this, 'blur', 'onBlur_');
      this.removeObservers_();
      if (this.firstControl_) {
        this.unlisten(this.firstControl_, 'keydown', 'onFirstControlKeydown_');
      }
      if (this.row_) {
        this.row_.destroy();
      }
    },

    /** @return {!FocusRow} */
    getFocusRow() {
      return assert(this.row_);
    },

    /** @private */
    updateFirstControl_() {
      const newFirstControl = this.row_.getFirstFocusable();
      if (newFirstControl === this.firstControl_) {
        return;
      }

      if (this.firstControl_) {
        this.unlisten(this.firstControl_, 'keydown', 'onFirstControlKeydown_');
      }
      this.firstControl_ = newFirstControl;
      if (this.firstControl_) {
        this.listen(
            /** @type {!Element} */ (this.firstControl_), 'keydown',
            'onFirstControlKeydown_');
      }
    },

    /** @private */
    removeObservers_() {
      if (this.controlObservers_.length > 0) {
        this.controlObservers_.forEach(observer => {
          observer.disconnect();
        });
      }
      this.controlObservers_ = [];
    },

    /** @private */
    addItems_() {
      this.ironListTabIndexChanged_();
      if (this.row_) {
        this.removeObservers_();
        this.row_.destroy();

        const controls = this.root.querySelectorAll('[focus-row-control]');

        controls.forEach(control => {
          this.row_.addItem(
              control.getAttribute('focus-type'),
              /** @type {!HTMLElement} */
              (FocusRow.getFocusableElement(control)));
          this.addMutationObservers_(assert(control));
        });
        this.updateFirstControl_();
      }
    },

    /**
     * @return {!MutationObserver}
     * @private
     */
    createObserver_() {
      return new MutationObserver(mutations => {
        const mutation = mutations[0];
        if (mutation.attributeName === 'style' && mutation.oldValue) {
          const newStyle = window.getComputedStyle(
              /** @type {!Element} */ (mutation.target));
          const oldDisplayValue = mutation.oldValue.match(/^display:(.*)(?=;)/);
          const oldVisibilityValue =
              mutation.oldValue.match(/^visibility:(.*)(?=;)/);
          // Return early if display and visibility have not changed.
          if (oldDisplayValue &&
              newStyle.display === oldDisplayValue[1].trim() &&
              oldVisibilityValue &&
              newStyle.visibility === oldVisibilityValue[1].trim()) {
            return;
          }
        }
        this.updateFirstControl_();
      });
    },

    /**
     * The first focusable control changes if hidden, disabled, or style.display
     * changes for the control or any of its ancestors. Add mutation observers
     * to watch for these changes in order to ensure the first control keydown
     * listener is always on the correct element.
     * @param {!Element} control
     * @private
     */
    addMutationObservers_(control) {
      let current = control;
      while (current && current !== this.root) {
        const currentObserver = this.createObserver_();
        currentObserver.observe(current, {
          attributes: true,
          attributeFilter: ['hidden', 'disabled', 'style'],
          attributeOldValue: true,
        });
        this.controlObservers_.push(currentObserver);
        current = current.parentNode;
      }
    },

    /**
     * This function gets called when the row itself receives the focus event.
     * @param {!Event} e The focus event
     * @private
     */
    onFocus_(e) {
      if (this.mouseFocused_) {
        this.mouseFocused_ = false;  // Consume and reset flag.
        return;
      }

      // If focus is being restored from outside the item and the event is fired
      // by the list item itself, focus the first control so that the user can
      // tab through all the controls. When the user shift-tabs back to the row,
      // or focus is restored to the row from a dropdown on the last item, the
      // last child item will be focused before the row itself. Since this is
      // the desired behavior, do not shift focus to the first item in these
      // cases.
      const restoreFocusToFirst =
          this.listBlurred && e.composedPath()[0] === this;

      if (this.lastFocused && !restoreFocusToFirst) {
        focusWithoutInk(this.row_.getEquivalentElement(this.lastFocused));
      } else {
        const firstFocusable = assert(this.firstControl_);
        focusWithoutInk(firstFocusable);
      }
      this.listBlurred = false;
      this.isFocused = true;
    },

    /** @param {!KeyboardEvent} e */
    onFirstControlKeydown_(e) {
      if (e.shiftKey && e.key === 'Tab') {
        this.focus();
      }
    },

    /** @private */
    ironListTabIndexChanged_() {
      if (this.row_) {
        this.row_.makeActive(this.ironListTabIndex === 0);
      }

      // If a new row is being focused, reset listBlurred. This means an item
      // has been removed and iron-list is about to focus the next item.
      if (this.ironListTabIndex === 0) {
        this.listBlurred = false;
      }
    },

    /** @private */
    onMouseDown_() {
      this.mouseFocused_ = true;  // Set flag to not do any control-focusing.
    },

    /**
     * @param {!Event} e
     * @private
     */
    onBlur_(e) {
      // Reset focused flags since it's not active anymore.
      this.mouseFocused_ = false;
      this.isFocused = false;

      const node =
          e.relatedTarget ? /** @type {!Node} */ (e.relatedTarget) : null;
      if (!this.parentNode.contains(node)) {
        this.listBlurred = true;
      }
    },
  };

  /** @interface */
  export class FocusRowBehaviorInterface {
    constructor() {
      /** @type {string} */
      this.id;

      /** @type {boolean} */
      this.isFocused;

      /** @type {number} */
      this.focusRowIndex;

      /** @type {?HTMLElement} */
      this.lastFocused;

      /** @type {number} */
      this.ironListTabIndex;

      /** @type {boolean} */
      this.listBlurred;
    }

    /**
     * @param {number} newIndex
     * @param {number} oldIndex
     */
    focusRowIndexChanged(newIndex, oldIndex) {}

    /** @return {!FocusRow} */
    getFocusRow() {}
  }

