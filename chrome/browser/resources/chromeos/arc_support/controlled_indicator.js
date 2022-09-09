// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('cr.ui', function() {
  /** @const */ const BubbleButton = cr.ui.BubbleButton;

  /**
   * An indicator that can be placed on a UI element as a hint to the user that
   * the value is controlled by some external entity such as policy or an
   * extension.
   * @constructor
   * @extends {cr.ui.BubbleButton}
   */
  const ControlledIndicator = cr.ui.define('span');

  /**
   * Only a single bubble can be shown at a time. |bubble| holds a reference to
   * the bubble, if any.
   * @private
   */
  let bubble;

  ControlledIndicator.prototype = {
    __proto__: cr.ui.BubbleButton.prototype,

    /**
     * Decorates the base element to show the proper icon.
     */
    decorate() {
      cr.ui.BubbleButton.prototype.decorate.call(this);
      this.classList.add('controlled-setting-indicator');
    },

    /**
     * Shows an informational bubble displaying |content|.
     * @param {HTMLDivElement} content The content of the bubble.
     */
    showBubble(content) {
      this.hideBubble();

      bubble = new cr.ui.AutoCloseBubble();
      bubble.anchorNode = this.image;
      bubble.domSibling = this;
      bubble.arrowLocation = this.location;
      bubble.content = content;
      bubble.show();
    },

    /**
     * Hides the currently visible bubble, if any.
     */
    hideBubble() {
      if (bubble) {
        bubble.hide();
      }
    },

    /**
     * Returns a dictionary of the form { |controlled-by|: |bubbleText| }, where
     * |controlled-by| is a valid value of the controlled-by property (see
     * below), i.e. 'policy'. |bubbleText| is the default text to be shown for
     * UI items with this controlled-by property value. The default
     * implementation does not set any strings.
     * @return {Object}
     */
    getDefaultStrings() {
      return {};
    },

    /**
     * Returns the text shown in the bubble.
     * @return {string}
     */
    getBubbleText() {
      const defaultStrings = this.getDefaultStrings();
      let text = defaultStrings[this.controlledBy];

      if (this.hasAttribute('text' + this.controlledBy)) {
        text = this.getAttribute('text' + this.controlledBy);
      } else if (this.controlledBy === 'extension' && this['extensionName']) {
        text = defaultStrings['extensionWithName'];
      }

      return text || '';
    },

    /**
     * Returns the DOM tree for a showing the message |text|.
     * @param {string} text to be shown in the bubble.
     */
    createDomTree(text) {
      const content = document.createElement('div');
      content.textContent = text;
      return content;
    },

    /**
     * Open or close a bubble with further information about the pref.
     * @override
     */
    toggleBubble() {
      if (this.showingBubble) {
        this.hideBubble();
      } else {
        this.showBubble(this.createDomTree(this.getBubbleText()));
      }
    },
  };

  /**
   * The status of the associated preference:
   * - 'policy':            A specific value is enforced by policy.
   * - 'extension':         A specific value is enforced by an extension.
   * - 'recommended':       A value is recommended by policy. The user could
   *                        override this recommendation but has not done so.
   * - 'hasRecommendation': A value is recommended by policy. The user has
   *                        overridden this recommendation.
   * - 'owner':             A value is controlled by the owner of the device
   *                        (Chrome OS only).
   * - 'shared':            A value belongs to the primary user but can be
   *                        modified (Chrome OS only).
   * - unset:               The value is controlled by the user alone.
   */
  ControlledIndicator.prototype.controlledBy;
  Object.defineProperty(
      ControlledIndicator.prototype, 'controlledBy',
      cr.getPropertyDescriptor('controlledBy', cr.PropertyKind.BOOL_ATTR));

  return {ControlledIndicator: ControlledIndicator};
});
