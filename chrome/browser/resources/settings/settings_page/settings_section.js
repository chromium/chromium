// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-section' shows a paper material themed section with a header
 * which shows its page title.
 *
 * The section can expand vertically to fill its container's padding edge.
 *
 * Example:
 *
 *    <settings-section page-title="[[pageTitle]]" section="privacy">
 *      <!-- Insert your section controls here -->
 *    </settings-section>
 */

let SettingsSectionElement = Polymer({
  is: 'settings-section',

  properties: {
    /**
     * The section name should match a name specified in route.js. The
     * MainPageBehavior will expand this section if this section name matches
     * currentRoute.section.
     */
    section: String,

    /**
     * Title for the section header. Initialize so we can use the
     * getTitleHiddenStatus_ method for accessibility.
     */
    pageTitle: {
      type: String,
      value: '',
    },

    /**
     * A CSS attribute used for temporarily hiding a SETTINGS-SECTION for the
     * purposes of searching.
     */
    hiddenBySearch: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    /**
     * Original height of the collapsed section, used as the target height when
     * collapsing after being expanded.
     * TODO(michaelpg): Get the height dynamically when collapsing using the
     * card's main page.
     * @private
     */
    collapsedHeight_: {
      type: Number,
      value: NaN,
    },
  },

  /**
   * Freezes the section's height so its card can be removed from the flow
   * without affecting the layout of the surrounding sections.
   * @param {boolean} frozen True to freeze, false to unfreeze.
   */
  setFrozen: function(frozen) {
    const card = this.$.card;
    if (frozen) {
      this.style.height = this.clientHeight + 'px';

      const cardHeight = card.offsetHeight;
      const cardWidth = card.offsetWidth;
      // If the section is not displayed yet (e.g., navigated directly to a
      // sub-page), cardHeight and cardWidth are 0, so do not set the height or
      // width explicitly.
      if (cardHeight && cardWidth) {
        card.style.height = cardHeight + 'px';
        card.style.width = cardWidth + 'px';
      }

      // Place the section's card at its current position but removed from the
      // flow.
      card.style.top = card.getBoundingClientRect().top + 'px';
      this.classList.add('frozen');
    } else {
      // Restore the section to its normal height.
      if (!this.classList.contains('frozen'))
        return;
      this.classList.remove('frozen');
      this.$.card.style.top = '';
      this.$.card.style.height = '';
      this.$.card.style.width = '';
      this.style.height = '';
    }
  },

  /**
   * Calling this method fires the 'settings-section-expanded event'.
   */
  setExpanded_: function() {
    this.classList.add('expanded');
    this.fire('settings-section-expanded');
  },

  /**
   * @return {boolean} True if the section is currently rendered and not
   *     already expanded or transitioning.
   */
  canAnimateExpand: function() {
    return !this.classList.contains('expanding') &&
        !this.classList.contains('expanded') && this.$.card.clientHeight > 0;
  },

  immediateExpand: function(container) {
    // Target position is the container's top edge in the viewport.
    const containerTop = container.getBoundingClientRect().top;

    this.$.card.position = 'fixed';
    this.$.card.top = containerTop + 'px';
    this.$.card.height = 'calc(100% - ' + containerTop + 'px)';

    this.setExpanded_();
  },

  /**
   * Animates the section expanding to fill the container. The section is fixed
   * in the viewport during the animation, making it safe to adjust the rest of
   * the DOM after calling this. The section adds the "expanding" class while
   * the animation plays and "expanded" after it finishes.
   *
   * @param {!HTMLElement} container The scrolling container to fill.
   * @return {!settings.animation.Animation}
   */
  animateExpand: function(container) {
    // Set the section's height so its card can be removed from the flow
    // without affecting the surrounding sections during the animation.
    this.collapsedHeight_ = this.clientHeight;
    this.style.height = this.collapsedHeight_ + 'px';

    this.classList.add('expanding');

    // Start the card in place, at its distance from the container's padding.
    const startTop = this.$.card.getBoundingClientRect().top + 'px';
    const startHeight = this.$.card.clientHeight + 'px';

    // Target position is the container's top edge in the viewport.
    const containerTop = container.getBoundingClientRect().top;
    const endTop = containerTop + 'px';
    // The card should stretch from the bottom of the toolbar to the bottom of
    // the page. calc(100% - top) lets the card resize if the window resizes.
    const endHeight = 'calc(100% - ' + containerTop + 'px)';

    const animation =
        this.animateCard_('fixed', startTop, endTop, startHeight, endHeight);
    // The empty onRejected function prevents the promise from skipping forward
    // to the next then() with a rejection callback.
    animation.finished.then(this.setExpanded_.bind(this), () => {}).then(() => {
      // Unset these changes whether the animation finished or canceled.
      this.classList.remove('expanding');
      this.style.height = '';
    });
    return animation;
  },

  /**
   * @return {boolean} True if the section is currently expanded and we know
   *     what the collapsed height should be.
   */
  canAnimateCollapse: function() {
    return this.classList.contains('expanded') && this.clientHeight > 0 &&
        !Number.isNaN(this.collapsedHeight_);
  },

  /**
   * Prepares for the animation before the other sections become visible.
   * Call before animateCollapse().
   * @param {!HTMLElement} container
   */
  setUpAnimateCollapse: function(container) {
    // Prepare the dimensions and set position: fixed.
    this.$.card.style.width = this.$.card.clientWidth + 'px';
    this.$.card.style.height = this.$.card.clientHeight + 'px';
    this.$.card.style.top = container.getBoundingClientRect().top + 'px';
    this.$.card.style.position = 'fixed';

    // The section can now collapse back into its original height the page so
    // the other sections appear in the right places.
    this.classList.remove('expanded');
    this.classList.add('collapsing');
    this.style.height = this.collapsedHeight_ + 'px';
  },

  /**
   * Collapses an expanded section's card back into position in the main page.
   * Call after calling animateCollapse(), unhiding other content and scrolling.
   * @param {!HTMLElement} container The scrolling container the card fills.
   * @return {!settings.animation.Animation}
   */
  animateCollapse: function(container) {
    // Make the card position: absolute, so scrolling is less of a crapshoot.
    // First find the current distance between this section and the card using
    // fixed coordinates; the absolute distance will be the same.
    const fixedCardTop = this.$.card.getBoundingClientRect().top;
    const fixedSectionTop = this.getBoundingClientRect().top;
    const distance = fixedCardTop - fixedSectionTop;

    // The target position is right below our header.
    const headerStyle = getComputedStyle(this.$.header);
    const cardTargetTop = this.$.header.offsetHeight +
        parseFloat(headerStyle.marginBottom) +
        parseFloat(headerStyle.marginTop);

    // Start the card at its current height and distance from our top.
    const startTop = distance + 'px';
    const startHeight = this.$.card.style.height;

    // End at the bottom of our header.
    const endTop = cardTargetTop + 'px';
    const endHeight = (this.collapsedHeight_ - cardTargetTop) + 'px';

    // The card no longer needs position: fixed.
    this.$.card.style.position = '';

    // Collapse this section, animate the card into place, and remove its
    // other properties.
    const animation =
        this.animateCard_('absolute', startTop, endTop, startHeight, endHeight);
    this.$.card.style.width = '';
    this.$.card.style.height = '';
    this.$.card.style.top = '';

    animation.finished
        .then(
            () => {
              this.classList.remove('expanded');
            },
            function() {})
        .then(() => {
          // The card now determines the section's height automatically.
          this.style.height = '';
          this.classList.remove('collapsing');
        });
    return animation;
  },

  show: function() {
    this.setAttribute('tabindex', '-1');
    this.focus();
    this.scrollIntoView();
    listenOnce(this, ['blur', 'pointerdown'], () => {
      this.removeAttribute('tabindex');
    });
  },

  /**
   * Helper function to animate the card's position and height.
   * @param {string} position CSS position property.
   * @param {string} startTop Initial top value.
   * @param {string} endTop Target top value.
   * @param {string} startHeight Initial height value.
   * @param {string} endHeight Target height value.
   * @return {!settings.animation.Animation}
   * @private
   */
  animateCard_: function(position, startTop, endTop, startHeight, endHeight) {
    // Width does not change.
    const width = this.$.card.clientWidth + 'px';

    const startFrame = {
      position: position,
      width: width,
      top: startTop,
      height: startHeight,
    };

    const endFrame = {
      position: position,
      width: width,
      top: endTop,
      height: endHeight,
    };

    const options = /** @type {!KeyframeEffectOptions} */ ({
      duration: settings.animation.Timing.DURATION,
      easing: settings.animation.Timing.EASING,
    });

    return new settings.animation.Animation(
        this.$.card, [startFrame, endFrame], options);
  },

  /**
   * Get the value to which to set the aria-hidden attribute of the section
   * heading.
   * @return {boolean|string} A return value of false will not add aria-hidden
   *    while aria-hidden requires a string of 'true' to be hidden as per aria
   *    specs. This function ensures we have the right return type.
   * @private
   */
  getTitleHiddenStatus_: function() {
    return this.pageTitle ? false : 'true';
  }
});
