// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('cr.ui', function() {
  /**
   * An image button that brings up an informative bubble when activated by
   * keyboard or mouse.
   * @constructor
   * @extends {HTMLSpanElement}
   * @implements {EventListener}
   */
  const BubbleButton = cr.ui.define('span');

  BubbleButton.prototype = {
    __proto__: HTMLSpanElement.prototype,

    /**
     * Decorates the base element to show the proper icon.
     */
    decorate() {
      this.className = 'bubble-button';
      this.location = cr.ui.ArrowLocation.TOP_END;
      this.image = document.createElement('div');
      this.image.tabIndex = 0;
      this.image.setAttribute('role', 'button');
      this.image.addEventListener('click', this);
      this.image.addEventListener('keydown', this);
      this.image.addEventListener('mousedown', this);
      this.appendChild(this.image);
    },

    /**
     * Whether the button is currently showing a bubble.
     * @type {boolean}
     */
    get showingBubble() {
      return this.image.classList.contains('showing-bubble');
    },
    set showingBubble(showing) {
      this.image.classList.toggle('showing-bubble', showing);
    },

    /**
     * Handle mouse and keyboard events, allowing the user to open and close an
     * informative bubble.
     * @param {Event} event Mouse or keyboard event.
     */
    handleEvent(event) {
      switch (event.type) {
        // Toggle the bubble on left click. Let any other clicks propagate.
        case 'click':
          if (event.button !== 0) {
            return;
          }
          break;
        // Toggle the bubble when <Return> or <Space> is pressed. Let any other
        // key presses propagate.
        case 'keydown':
          switch (event.keyCode) {
            case 13:  // Return.
            case 32:  // Space.
              break;
            default:
              return;
          }
          break;
        // Blur focus when a mouse button is pressed, matching the behavior of
        // other Web UI elements.
        case 'mousedown':
          if (document.activeElement) {
            document.activeElement.blur();
          }
          event.preventDefault();
          return;
      }
      this.toggleBubble();
      event.preventDefault();
      event.stopPropagation();
    },

    /**
     * Abstract method: subclasses should overwrite it. There is no way to mark
     *     method as abstract for Closure Compiler, as of
     *     https://github.com/google/closure-compiler/issues/104.
     * @type {!Function|undefined}
     * @protected
     */
    toggleBubble: assertNotReached,
  };

  // Export.
  return {BubbleButton: BubbleButton};
});
