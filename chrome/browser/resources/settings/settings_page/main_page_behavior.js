// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Responds to route changes by expanding, collapsing, or scrolling to sections
 * on the page. Expanded sections take up the full height of the container. At
 * most one section should be expanded at any given time.
 * @polymerBehavior MainPageBehavior
 */
const MainPageBehaviorImpl = {
  properties: {
    /**
     * Help CSS to alter style during the horizontal swipe animation.
     * Note that this is unrelated to the |currentAnimation_| (which refers to
     * the vertical expand animation).
     */
    isSubpageAnimating: {
      reflectToAttribute: true,
      type: Boolean,
    },

    /**
     * Whether a search operation is in progress or previous search results are
     * being displayed.
     * @private {boolean}
     */
    inSearchMode: {
      type: Boolean,
      value: false,
      observer: 'inSearchModeChanged_',
    },
  },

  /** @type {?HTMLElement} The scrolling container. */
  scroller: null,

  listeners: {'neon-animation-finish': 'onNeonAnimationFinish_'},

  /** @override */
  attached: function() {
    this.scroller = this.domHost ? this.domHost.parentNode : document.body;
  },

  /**
   * Remove the is-animating attribute once the animation is complete.
   * This may catch animations finishing more often than needed, which is not
   * known to cause any issues (e.g. when animating from a shallower page to a
   * deeper page; or when transitioning to the main page).
   * @private
   */
  onNeonAnimationFinish_: function() {
    this.isSubpageAnimating = false;
  },

  /**
   * @param {!settings.Route} newRoute
   * @param {settings.Route} oldRoute
   */
  currentRouteChanged: function(newRoute, oldRoute) {
    const oldRouteWasSection = !!oldRoute && !!oldRoute.parent &&
        !!oldRoute.section && oldRoute.parent.section != oldRoute.section;

    if (this.scroller) {
      // When navigating from a section to the root route, we just need to
      // scroll to the top, and can early exit afterwards.
      if (oldRouteWasSection && newRoute == settings.routes.BASIC) {
        this.scroller.scrollTop = 0;
        return;
      }

      // When navigating to the About page, we need to scroll to the top, and
      // still do the rest of section management.
      if (newRoute == settings.routes.ABOUT)
        this.scroller.scrollTop = 0;
    }

    // Scroll to the section except for back/forward. Also scroll for any
    // in-page back/forward navigations (from a section or the root page).
    // Also always scroll when coming from either the About or root page.
    const scrollToSection = !settings.lastRouteChangeWasPopstate() ||
        oldRouteWasSection || oldRoute == settings.routes.BASIC ||
        oldRoute == settings.routes.ABOUT;

    // TODO(dschuyler): This doesn't set the flag in the case of going to or
    // from the main page. It seems sensible to set the flag in those cases,
    // unfortunately bug 708465 happens. Figure out why that is and then set
    // this flag more broadly.
    if (oldRoute && oldRoute.isSubpage() && newRoute.isSubpage())
      this.isSubpageAnimating = true;

    // For previously uncreated pages (including on first load), allow the page
    // to render before scrolling to or expanding the section.
    if (!oldRoute) {
      this.fire('hide-container');
      setTimeout(() => {
        this.fire('show-container');
        this.tryTransitionToSection_(scrollToSection, true);
      });
    } else if (this.scrollHeight == 0) {
      setTimeout(this.tryTransitionToSection_.bind(this, scrollToSection));
    } else {
      this.tryTransitionToSection_(scrollToSection);
    }
  },

  /**
   * When exiting search mode, we need to make another attempt to scroll to
   * the correct section, since it has just been re-rendered.
   * @private
   */
  inSearchModeChanged_: function(inSearchMode) {
    if (!this.isAttached)
      return;

    if (!inSearchMode)
      this.tryTransitionToSection_(!settings.lastRouteChangeWasPopstate());
  },

  /**
   * If possible, transitions to the current route's section (by expanding or
   * scrolling to it). If another transition is running, finishes or cancels
   * that one, then schedules this function again. This ensures the current
   * section is quickly shown, without getting the page into a broken state --
   * if currentRoute changes in between calls, just transition to the new route.
   * @param {boolean} scrollToSection
   * @param {boolean=} immediate Whether to instantly expand instead of animate.
   * @private
   */
  tryTransitionToSection_: function(scrollToSection, immediate) {
    const currentRoute = settings.getCurrentRoute();
    const currentSection = this.getSection(currentRoute.section);

    // If an animation is already playing, try finishing or canceling it.
    if (this.currentAnimation_) {
      this.maybeStopCurrentAnimation_();
      // Either way, this function will be called again once the current
      // animation ends.
      return;
    }

    let promise;
    const expandedSection = /** @type {?SettingsSectionElement} */ (
        this.$$('settings-section.expanded'));
    if (expandedSection) {
      // If the section shouldn't be expanded, collapse it.
      if (!currentRoute.isSubpage() || expandedSection != currentSection) {
        promise = this.collapseSection_(expandedSection);
      } else {
        // Scroll to top while sliding to another subpage.
        this.scroller.scrollTop = 0;
      }
    } else if (currentSection) {
      // Expand the section into a subpage or scroll to it on the main page.
      if (currentRoute.isSubpage()) {
        if (immediate)
          this.expandSectionImmediate_(currentSection);
        else
          promise = this.expandSection_(currentSection);
      } else if (scrollToSection) {
        currentSection.show();
      }
    } else if (
        this.tagName == 'SETTINGS-BASIC-PAGE' && settings.routes.ADVANCED &&
        settings.routes.ADVANCED.contains(currentRoute) &&
        // Need to exclude routes that correspond to 'non-sectioned' children of
        // ADVANCED, otherwise tryTransitionToSection_ will recurse endlessly.
        !currentRoute.isNavigableDialog) {
      assert(currentRoute.section);
      // Hide the container again while Advanced Page template is being loaded.
      this.fire('hide-container');
      promise = this.$$('#advancedPageTemplate').get();
    }

    // When this animation ends, another may be necessary. Call this function
    // again after the promise resolves.
    if (promise) {
      promise.then(this.tryTransitionToSection_.bind(this, scrollToSection))
          .then(() => {
            this.fire('show-container');
          });
    }
  },

  /**
   * If the current animation is inconsistent with the current route, stops the
   * animation by finishing or canceling it so the new route can be animated to.
   * @private
   */
  maybeStopCurrentAnimation_: function() {
    const currentRoute = settings.getCurrentRoute();
    const animatingSection = /** @type {?SettingsSectionElement} */ (
        this.$$('settings-section.expanding, settings-section.collapsing'));
    assert(animatingSection);

    if (animatingSection.classList.contains('expanding')) {
      // Cancel the animation to go back to the main page if the animating
      // section shouldn't be expanded.
      if (animatingSection.section != currentRoute.section ||
          !currentRoute.isSubpage()) {
        this.currentAnimation_.cancel();
      }
      // Otherwise, let the expand animation continue.
      return;
    }

    assert(animatingSection.classList.contains('collapsing'));
    if (!currentRoute.isSubpage())
      return;

    // If the collapsing section actually matches the current route's section,
    // we can just cancel the animation to re-expand the section.
    if (animatingSection.section == currentRoute.section) {
      this.currentAnimation_.cancel();
      return;
    }

    // The current route is a subpage, so that section needs to expand.
    // Immediately finish the current collapse animation so that can happen.
    this.currentAnimation_.finish();
  },

  /**
   * Immediately expand the card in |section| to fill the page.
   * @param {!SettingsSectionElement} section
   * @private
   */
  expandSectionImmediate_: function(section) {
    assert(this.scroller);
    section.immediateExpand(this.scroller);
    this.finishedExpanding_(section);
    // TODO(scottchen): iron-list inside subpages need this to render correctly.
    this.fire('resize');
  },

  /**
   * Animates the card in |section|, expanding it to fill the page.
   * @param {!SettingsSectionElement} section
   * @return {!Promise} Resolved when the transition is finished or canceled.
   * @private
   */
  expandSection_: function(section) {
    assert(this.scroller);
    if (!section.canAnimateExpand()) {
      // Try to wait for the section to be created.
      return new Promise(function(resolve, reject) {
        setTimeout(resolve);
      });
    }

    // Save the scroller position before freezing it.
    this.origScrollTop_ = this.scroller.scrollTop;
    this.fire('freeze-scroll', true);

    // Freeze the section's height so its card can be removed from the flow.
    section.setFrozen(true);

    this.currentAnimation_ = section.animateExpand(this.scroller);

    return this.currentAnimation_.finished
        .then(
            () => {
              this.finishedExpanding_(section);
            },
            () => {
              // The animation was canceled; restore the section and scroll
              // position.
              section.setFrozen(false);
              this.scroller.scrollTop = this.origScrollTop_;
            })
        .then(() => {
          this.fire('freeze-scroll', false);
          this.currentAnimation_ = null;
        });
  },

  /** @private */
  finishedExpanding_: function(section) {
    // Hide other sections and scroll to the top of the subpage.
    this.classList.add('showing-subpage');
    this.toggleOtherSectionsHidden_(section.section, true);
    this.scroller.scrollTop = 0;
    section.setFrozen(false);

    // Notify that the page is fully expanded.
    this.fire('subpage-expand');
  },

  /**
   * Animates the card in |section|, collapsing it back into its section.
   * @param {!SettingsSectionElement} section
   * @return {!Promise} Resolved when the transition is finished or canceled.
   */
  collapseSection_: function(section) {
    assert(this.scroller);
    assert(section.classList.contains('expanded'));

    // Don't animate the collapse if we are transitioning between Basic/Advanced
    // and About, since the section won't be visible.
    const needAnimate =
        settings.routes.ABOUT.contains(settings.getCurrentRoute()) ==
        (section.domHost.tagName == 'SETTINGS-ABOUT-PAGE');

    // Animate the collapse if the section knows the original height, except
    // when switching between Basic/Advanced and About.
    const shouldAnimateCollapse = needAnimate && section.canAnimateCollapse();
    if (shouldAnimateCollapse) {
      this.fire('freeze-scroll', true);
      // Do the initial collapse setup, which takes the section out of the flow,
      // before showing everything.
      section.setUpAnimateCollapse(this.scroller);
    } else {
      section.classList.remove('expanded');
    }

    // Show everything.
    this.toggleOtherSectionsHidden_(section.section, false);
    this.classList.remove('showing-subpage');

    if (!shouldAnimateCollapse) {
      // Finish by restoring the section into the page.
      section.setFrozen(false);
      return Promise.resolve();
    }

    // Play the actual collapse animation.
    return new Promise((resolve, reject) => {
      // Wait for the other sections to show up so we can scroll properly.
      setTimeout(() => {
        const newSection = settings.getCurrentRoute().section &&
            this.getSection(settings.getCurrentRoute().section);

        // Scroll to the new section or the original position.
        if (newSection && !settings.lastRouteChangeWasPopstate() &&
            !settings.getCurrentRoute().isSubpage()) {
          newSection.scrollIntoView();
        } else {
          this.scroller.scrollTop = this.origScrollTop_;
        }

        this.currentAnimation_ = section.animateCollapse(
            /** @type {!HTMLElement} */ (this.scroller));

        this.currentAnimation_.finished
            .catch(() => {
              // The collapse was canceled, so the page is showing a subpage
              // still.
              this.fire('subpage-expand');
            })
            .then(() => {
              // Clean up after the animation succeeds or cancels.
              section.setFrozen(false);
              section.classList.remove('collapsing');
              this.fire('freeze-scroll', false);
              this.currentAnimation_ = null;
              resolve();
            });
      });
    });
  },

  /**
   * Hides or unhides the sections not being expanded.
   * @param {string} sectionName The section to keep visible.
   * @param {boolean} hidden Whether the sections should be hidden.
   * @private
   */
  toggleOtherSectionsHidden_: function(sectionName, hidden) {
    const sections =
        Polymer.dom(this.root).querySelectorAll('settings-section');
    for (let i = 0; i < sections.length; i++)
      sections[i].hidden = hidden && (sections[i].section != sectionName);
  },

  /**
   * Helper function to get a section from the local DOM.
   * @param {string} section Section name of the element to get.
   * @return {?SettingsSectionElement}
   */
  getSection: function(section) {
    if (!section)
      return null;
    return /** @type {?SettingsSectionElement} */ (
        this.$$('settings-section[section="' + section + '"]'));
  },
};

/** @polymerBehavior */
const MainPageBehavior = [
  settings.RouteObserverBehavior,
  MainPageBehaviorImpl,
];
