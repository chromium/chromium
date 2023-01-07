// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// require: event_tracker.js

cr.define('cr.ui', function() {
  /**
   * The arrow location specifies how the arrow and bubble are positioned in
   * relation to the anchor node.
   * @enum {string}
   */
  const ArrowLocation = {
    // The arrow is positioned at the top and the start of the bubble. In left
    // to right mode this is the top left. The entire bubble is positioned below
    // the anchor node.
    TOP_START: 'top-start',
    // The arrow is positioned at the top and the end of the bubble. In left to
    // right mode this is the top right. The entire bubble is positioned below
    // the anchor node.
    TOP_END: 'top-end',
    // The arrow is positioned at the bottom and the start of the bubble. In
    // left to right mode this is the bottom left. The entire bubble is
    // positioned above the anchor node.
    BOTTOM_START: 'bottom-start',
    // The arrow is positioned at the bottom and the end of the bubble. In
    // left to right mode this is the bottom right. The entire bubble is
    // positioned above the anchor node.
    BOTTOM_END: 'bottom-end',
  };

  /**
   * The bubble alignment specifies the position of the bubble in relation to
   * the anchor node.
   * @enum {string}
   */
  const BubbleAlignment = {
    // The bubble is positioned just above or below the anchor node (as
    // specified by the arrow location) so that the arrow points at the midpoint
    // of the anchor.
    ARROW_TO_MID_ANCHOR: 'arrow-to-mid-anchor',
    // The bubble is positioned just above or below the anchor node (as
    // specified by the arrow location) so that its reference edge lines up with
    // the edge of the anchor.
    BUBBLE_EDGE_TO_ANCHOR_EDGE: 'bubble-edge-anchor-edge',
    // The bubble is positioned so that it is entirely within view and does not
    // obstruct the anchor element, if possible. The specified arrow location is
    // taken into account as the preferred alignment but may be overruled if
    // there is insufficient space (see BubbleBase.reposition for the exact
    // placement algorithm).
    ENTIRELY_VISIBLE: 'entirely-visible',
  };

  /**
   * Abstract base class that provides common functionality for implementing
   * free-floating informational bubbles with a triangular arrow pointing at an
   * anchor node.
   * @constructor
   * @extends {HTMLDivElement}
   * @implements {EventListener}
   */
  const BubbleBase = cr.ui.define('div');

  /**
   * The horizontal distance between the tip of the arrow and the reference edge
   * of the bubble (as specified by the arrow location). In pixels.
   * @type {number}
   * @const
   */
  BubbleBase.ARROW_OFFSET = 30;

  /**
   * Minimum horizontal spacing between edge of bubble and edge of viewport
   * (when using the ENTIRELY_VISIBLE alignment). In pixels.
   * @type {number}
   * @const
   */
  BubbleBase.MIN_VIEWPORT_EDGE_MARGIN = 2;

  /**
   * This is used to create TrustedHTML.
   * @type {!TrustedTypePolicy}
   */
  const staticHtmlPolicy = trustedTypes.createPolicy('cr-ui-bubble-js-static', {
    createHTML: () => {
      return '<div class="bubble-content"></div>' +
          '<div class="bubble-shadow"></div>' +
          '<div class="bubble-arrow"></div>';
    },
  });

  BubbleBase.prototype = {
    // Set up the prototype chain.
    __proto__: HTMLDivElement.prototype,

    /**
     * @type {Node}
     * @private
     */
    anchorNode_: null,

    /**
     * Initialization function for the cr.ui framework.
     */
    decorate() {
      this.className = 'bubble';
      // TODO(Jun.Kokatsu@microsoft.com): remove an empty string argument
      // once supported.
      // https://github.com/w3c/webappsec-trusted-types/issues/278
      this.innerHTML = staticHtmlPolicy.createHTML('');
      this.hidden = true;
      this.bubbleAlignment = cr.ui.BubbleAlignment.ENTIRELY_VISIBLE;
    },

    /**
     * Set the anchor node, i.e. the node that this bubble points at. Only
     * available when the bubble is not being shown.
     * @param {HTMLElement} node The new anchor node.
     */
    set anchorNode(node) {
      if (!this.hidden) {
        return;
      }

      this.anchorNode_ = node;
    },

    /**
     * Set the conent of the bubble. Only available when the bubble is not being
     * shown.
     * @param {HTMLElement} node The root node of the new content.
     */
    set content(node) {
      if (!this.hidden) {
        return;
      }

      const bubbleContent = this.querySelector('.bubble-content');
      bubbleContent.innerHTML = trustedTypes.emptyHTML;
      bubbleContent.appendChild(node);
    },

    /**
     * Set the arrow location. Only available when the bubble is not being
     * shown.
     * @param {cr.ui.ArrowLocation} location The new arrow location.
     */
    set arrowLocation(location) {
      if (!this.hidden) {
        return;
      }

      this.arrowAtRight_ = location === cr.ui.ArrowLocation.TOP_END ||
          location === cr.ui.ArrowLocation.BOTTOM_END;
      if (document.documentElement.dir === 'rtl') {
        this.arrowAtRight_ = !this.arrowAtRight_;
      }
      this.arrowAtTop_ = location === cr.ui.ArrowLocation.TOP_START ||
          location === cr.ui.ArrowLocation.TOP_END;
    },

    /**
     * Set the bubble alignment. Only available when the bubble is not being
     * shown.
     * @param {cr.ui.BubbleAlignment} alignment The new bubble alignment.
     */
    set bubbleAlignment(alignment) {
      if (!this.hidden) {
        return;
      }

      this.bubbleAlignment_ = alignment;
    },

    /**
     * Update the position of the bubble. Whenever the layout may have changed,
     * the bubble should either be repositioned by calling this function or
     * hidden so that it does not point to a nonsensical location on the page.
     */
    reposition() {
      const documentWidth = document.documentElement.clientWidth;
      const documentHeight = document.documentElement.clientHeight;
      const anchor = this.anchorNode_.getBoundingClientRect();
      const anchorMid = (anchor.left + anchor.right) / 2;
      const bubble = this.getBoundingClientRect();
      const arrow = this.querySelector('.bubble-arrow').getBoundingClientRect();

      let left;
      let top;
      if (this.bubbleAlignment_ === cr.ui.BubbleAlignment.ENTIRELY_VISIBLE) {
        // Work out horizontal placement. The bubble is initially positioned so
        // that the arrow tip points toward the midpoint of the anchor and is
        // BubbleBase.ARROW_OFFSET pixels from the reference edge and (as
        // specified by the arrow location). If the bubble is not entirely
        // within view, it is then shifted, preserving the arrow tip position.
        left = this.arrowAtRight_ ?
            anchorMid + BubbleBase.ARROW_OFFSET - bubble.width :
            anchorMid - BubbleBase.ARROW_OFFSET;
        const maxLeftPos =
            documentWidth - bubble.width - BubbleBase.MIN_VIEWPORT_EDGE_MARGIN;
        const minLeftPos = BubbleBase.MIN_VIEWPORT_EDGE_MARGIN;
        if (document.documentElement.dir === 'rtl') {
          left = Math.min(Math.max(left, minLeftPos), maxLeftPos);
        } else {
          left = Math.max(Math.min(left, maxLeftPos), minLeftPos);
        }
        const arrowTip = Math.min(
            Math.max(
                arrow.width / 2,
                this.arrowAtRight_ ? left + bubble.width - anchorMid :
                                     anchorMid - left),
            bubble.width - arrow.width / 2);

        // Work out the vertical placement, attempting to fit the bubble
        // entirely into view. The following placements are considered in
        // decreasing order of preference:
        // * Outside the anchor, arrow tip touching the anchor (arrow at
        //   top/bottom as specified by the arrow location).
        // * Outside the anchor, arrow tip touching the anchor (arrow at
        //   bottom/top, opposite the specified arrow location).
        // * Outside the anchor, arrow tip overlapping the anchor (arrow at
        //   top/bottom as specified by the arrow location).
        // * Outside the anchor, arrow tip overlapping the anchor (arrow at
        //   bottom/top, opposite the specified arrow location).
        // * Overlapping the anchor.
        const offsetTop = Math.min(
            documentHeight - anchor.bottom - bubble.height, arrow.height / 2);
        const offsetBottom =
            Math.min(anchor.top - bubble.height, arrow.height / 2);
        if (offsetTop < 0 && offsetBottom < 0) {
          top = 0;
          this.updateArrowPosition_(false, false, arrowTip);
        } else if (
            offsetTop > offsetBottom ||
            offsetTop === offsetBottom && this.arrowAtTop_) {
          top = anchor.bottom + offsetTop;
          this.updateArrowPosition_(true, true, arrowTip);
        } else {
          top = anchor.top - bubble.height - offsetBottom;
          this.updateArrowPosition_(true, false, arrowTip);
        }
      } else {
        if (this.bubbleAlignment_ ===
            cr.ui.BubbleAlignment.BUBBLE_EDGE_TO_ANCHOR_EDGE) {
          left = this.arrowAtRight_ ? anchor.right - bubble.width : anchor.left;
        } else {
          left = this.arrowAtRight_ ?
              anchorMid - this.clientWidth + BubbleBase.ARROW_OFFSET :
              anchorMid - BubbleBase.ARROW_OFFSET;
        }
        top = this.arrowAtTop_ ?
            anchor.bottom + arrow.height / 2 :
            anchor.top - this.clientHeight - arrow.height / 2;
        this.updateArrowPosition_(
            true, this.arrowAtTop_, BubbleBase.ARROW_OFFSET);
      }

      this.style.left = left + 'px';
      this.style.top = top + 'px';
    },

    /**
     * Show the bubble.
     */
    show() {
      if (!this.hidden) {
        return;
      }

      this.attachToDOM_();
      this.hidden = false;
      this.reposition();

      const doc = assert(this.ownerDocument);
      this.eventTracker_ = new EventTracker();
      this.eventTracker_.add(doc, 'keydown', this, true);
      this.eventTracker_.add(doc, 'mousedown', this, true);
    },

    /**
     * Hide the bubble.
     */
    hide() {
      if (this.hidden) {
        return;
      }

      this.eventTracker_.removeAll();
      this.hidden = true;
      this.parentNode.removeChild(this);
    },

    /**
     * Handle keyboard events, dismissing the bubble if necessary.
     * @param {Event} event The event.
     */
    handleEvent(event) {
      // Close the bubble when the user presses <Esc>.
      if (event.type === 'keydown' && event.keyCode === 27) {
        this.hide();
        event.preventDefault();
        event.stopPropagation();
      }
    },

    /**
     * Attach the bubble to the document's DOM.
     * @private
     */
    attachToDOM_() {
      document.body.appendChild(this);
    },

    /**
     * Update the arrow so that it appears at the correct position.
     * @param {boolean} visible Whether the arrow should be visible.
     * @param {boolean} atTop Whether the arrow should be at the top of the
     * bubble.
     * @param {number} tipOffset The horizontal distance between the tip of the
     * arrow and the reference edge of the bubble (as specified by the arrow
     * location).
     * @private
     */
    updateArrowPosition_(visible, atTop, tipOffset) {
      const bubbleArrow = this.querySelector('.bubble-arrow');
      bubbleArrow.hidden = !visible;
      if (!visible) {
        return;
      }

      let edgeOffset = (-bubbleArrow.clientHeight / 2) + 'px';
      bubbleArrow.style.top = atTop ? edgeOffset : 'auto';
      bubbleArrow.style.bottom = atTop ? 'auto' : edgeOffset;

      edgeOffset = (tipOffset - bubbleArrow.offsetWidth / 2) + 'px';
      bubbleArrow.style.left = this.arrowAtRight_ ? 'auto' : edgeOffset;
      bubbleArrow.style.right = this.arrowAtRight_ ? edgeOffset : 'auto';
    },
  };

  /**
   * A bubble that remains open until the user explicitly dismisses it or clicks
   * outside the bubble after it has been shown for at least the specified
   * amount of time (making it less likely that the user will unintentionally
   * dismiss the bubble). The bubble repositions itself on layout changes.
   * @constructor
   * @extends {cr.ui.BubbleBase}
   */
  const Bubble = cr.ui.define('div');

  Bubble.prototype = {
    // Set up the prototype chain.
    __proto__: BubbleBase.prototype,

    /**
     * Initialization function for the cr.ui framework.
     */
    decorate() {
      BubbleBase.prototype.decorate.call(this);

      const close = document.createElement('div');
      close.className = 'bubble-close';
      this.insertBefore(close, this.querySelector('.bubble-content'));

      this.handleCloseEvent = this.hide;
      this.deactivateToDismissDelay_ = 0;
      this.bubbleAlignment = cr.ui.BubbleAlignment.ARROW_TO_MID_ANCHOR;
    },

    /**
     * Handler for close events triggered when the close button is clicked. By
     * default, set to this.hide. Only available when the bubble is not being
     * shown.
     * @param {function(): *} handler The new handler, a function with no
     *     parameters.
     */
    set handleCloseEvent(handler) {
      if (!this.hidden) {
        return;
      }

      this.handleCloseEvent_ = handler;
    },

    /**
     * Set the delay before the user is allowed to click outside the bubble to
     * dismiss it. Using a delay makes it less likely that the user will
     * unintentionally dismiss the bubble.
     * @param {number} delay The delay in milliseconds.
     */
    set deactivateToDismissDelay(delay) {
      this.deactivateToDismissDelay_ = delay;
    },

    /**
     * Hide or show the close button.
     * @param {boolean} isVisible True if the close button should be visible.
     */
    set closeButtonVisible(isVisible) {
      this.querySelector('.bubble-close').hidden = !isVisible;
    },

    /**
     * Show the bubble.
     */
    show() {
      if (!this.hidden) {
        return;
      }

      BubbleBase.prototype.show.call(this);

      this.showTime_ = Date.now();
      this.eventTracker_.add(window, 'resize', this.reposition.bind(this));
    },

    /**
     * Handle keyboard and mouse events, dismissing the bubble if necessary.
     * @param {Event} event The event.
     * @suppress {checkTypes}
     * TODO(vitalyp): remove suppression when the extern
     * Node.prototype.contains() will be fixed.
     */
    handleEvent(event) {
      BubbleBase.prototype.handleEvent.call(this, event);

      if (event.type === 'mousedown') {
        // Dismiss the bubble when the user clicks on the close button.
        if (event.target === this.querySelector('.bubble-close')) {
          this.handleCloseEvent_();
          // Dismiss the bubble when the user clicks outside it after the
          // specified delay has passed.
        } else if (
            !this.contains(event.target) &&
            Date.now() - this.showTime_ >= this.deactivateToDismissDelay_) {
          this.hide();
        }
      }
    },
  };

  /**
   * A bubble that closes automatically when the user clicks or moves the focus
   * outside the bubble and its target element, scrolls the underlying document
   * or resizes the window.
   * @constructor
   * @extends {cr.ui.BubbleBase}
   */
  const AutoCloseBubble = cr.ui.define('div');

  AutoCloseBubble.prototype = {
    // Set up the prototype chain.
    __proto__: BubbleBase.prototype,

    /**
     * Initialization function for the cr.ui framework.
     */
    decorate() {
      BubbleBase.prototype.decorate.call(this);
      this.classList.add('auto-close-bubble');
    },

    /**
     * Set the DOM sibling node, i.e. the node as whose sibling the bubble
     * should join the DOM to ensure that focusable elements inside the bubble
     * follow the target element in the document's tab order. Only available
     * when the bubble is not being shown.
     * @param {HTMLElement} node The new DOM sibling node.
     */
    set domSibling(node) {
      if (!this.hidden) {
        return;
      }

      this.domSibling_ = node;
    },

    /**
     * Show the bubble.
     */
    show() {
      if (!this.hidden) {
        return;
      }

      BubbleBase.prototype.show.call(this);
      this.domSibling_.showingBubble = true;

      const doc = this.ownerDocument;
      this.eventTracker_.add(doc, 'click', this, true);
      this.eventTracker_.add(doc, 'mousewheel', this, true);
      this.eventTracker_.add(doc, 'scroll', this, true);
      this.eventTracker_.add(doc, 'elementFocused', this, true);
      this.eventTracker_.add(window, 'resize', this);
    },

    /**
     * Hide the bubble.
     */
    hide() {
      BubbleBase.prototype.hide.call(this);
      this.domSibling_.showingBubble = false;
    },

    /**
     * Handle events, closing the bubble when the user clicks or moves the focus
     * outside the bubble and its target element, scrolls the underlying
     * document or resizes the window.
     * @param {Event} event The event.
     * @suppress {checkTypes}
     * TODO(vitalyp): remove suppression when the extern
     * Node.prototype.contains() will be fixed.
     */
    handleEvent(event) {
      BubbleBase.prototype.handleEvent.call(this, event);

      let target;
      switch (event.type) {
        // Close the bubble when the user clicks outside it, except if it is a
        // left-click on the bubble's target element (allowing the target to
        // handle the event and close the bubble itself).
        case 'mousedown':
        case 'click':
          target = assertInstanceof(event.target, Node);
          if (event.button === 0 && this.anchorNode_.contains(target)) {
            break;
          }
        // Close the bubble when the underlying document is scrolled.
        case 'mousewheel':
        case 'scroll':
          target = assertInstanceof(event.target, Node);
          if (this.contains(target)) {
            break;
          }
        // Close the bubble when the window is resized.
        case 'resize':
          this.hide();
          break;
        // Close the bubble when the focus moves to an element that is not the
        // bubble target and is not inside the bubble.
        case 'elementFocused':
          target = assertInstanceof(event.target, Node);
          if (!this.anchorNode_.contains(target) && !this.contains(target)) {
            this.hide();
          }
          break;
      }
    },

    /**
     * Attach the bubble to the document's DOM, making it a sibling of the
     * |domSibling_| so that focusable elements inside the bubble follow the
     * target element in the document's tab order.
     * @private
     */
    attachToDOM_() {
      const parent = this.domSibling_.parentNode;
      parent.insertBefore(this, this.domSibling_.nextSibling);
    },
  };

  return {
    ArrowLocation: ArrowLocation,
    AutoCloseBubble: AutoCloseBubble,
    BubbleAlignment: BubbleAlignment,
    BubbleBase: BubbleBase,
    Bubble: Bubble,
  };
});
