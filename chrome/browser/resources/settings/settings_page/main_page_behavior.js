// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('settings');

/**
 * @enum {string}
 * A categorization of every possible Settings URL, necessary for implementing
 * a finite state machine.
 */
settings.RouteState = {
  // Initial state before anything has loaded yet.
  INITIAL: 'initial',
  // A dialog that has a dedicated URL (e.g. /importData).
  DIALOG: 'dialog',
  // A section (basically a scroll position within the top level page, e.g,
  // /appearance.
  SECTION: 'section',
  // A subpage, or sub-subpage e.g, /searchEngins.
  SUBPAGE: 'subpage',
  // The top level Settings page, '/'.
  TOP_LEVEL: 'top-level',
};

cr.define('settings', function() {
  const RouteState = settings.RouteState;

  /**
   * @param {?settings.Route} route
   * @return {!settings.RouteState}
   */
  function classifyRoute(route) {
    if (!route) {
      return RouteState.INITIAL;
    }
    if (route === settings.routes.BASIC || route === settings.routes.ABOUT) {
      return RouteState.TOP_LEVEL;
    }
    if (route.isSubpage()) {
      return RouteState.SUBPAGE;
    }
    if (route.isNavigableDialog) {
      return RouteState.DIALOG;
    }
    return RouteState.SECTION;
  }

  /**
   * Responds to route changes by expanding, collapsing, or scrolling to
   * sections on the page. Expanded sections take up the full height of the
   * container. At most one section should be expanded at any given time.
   * @polymerBehavior
   */
  const MainPageBehavior = {
    properties: {
      /**
       * Whether a search operation is in progress or previous search results
       * are being displayed.
       * @private {boolean}
       */
      inSearchMode: {
        type: Boolean,
        value: false,
        observer: 'inSearchModeChanged_',
      },
    },

    /** @type {?HTMLElement} */
    scroller: null,

    /**
     * A map holding all valid state transitions.
     * @private {!Map<!settings.RouteState, !settings.RouteState>}
     */
    validTransitions_: (function() {
      const allStates = new Set([
        RouteState.DIALOG,
        RouteState.SECTION,
        RouteState.SUBPAGE,
        RouteState.TOP_LEVEL,
      ]);

      return new Map([
        [RouteState.INITIAL, allStates],
        [
          RouteState.DIALOG, new Set([
            RouteState.SECTION,
            RouteState.SUBPAGE,
            RouteState.TOP_LEVEL,
          ])
        ],
        [RouteState.SECTION, allStates],
        [RouteState.SUBPAGE, allStates],
        [RouteState.TOP_LEVEL, allStates],
      ]);
    })(),

    /** @override */
    attached: function() {
      this.scroller = this.domHost ? this.domHost.parentNode : document.body;
    },

    /**
     * Method to be defined by users of MainPageBehavior.
     * @param {!settings.Route} route
     * @return {boolean} Whether the given route is part of |this| page.
     */
    containsRoute: function(route) {
      return false;
    },

    /**
     * @param {boolean} current
     * @param {boolean} previous
     * @private
     */
    inSearchModeChanged_: function(current, previous) {
      // Ignore 1st occurrence which happens while the element is being
      // initialized.
      if (previous === undefined) {
        return;
      }

      if (!this.inSearchMode) {
        const route = settings.getCurrentRoute();
        if (this.containsRoute(route) &&
            classifyRoute(route) === RouteState.SECTION) {
          // Re-fire the showing-section event to trigger settings-main
          // recalculation of the overscroll, now that sections are not
          // hidden-by-search.
          this.fire('showing-section', this.getSection(route.section));
        }
      }
    },

    /**
     * @param {!settings.Route} route
     * @return {boolean}
     * @private
     */
    shouldExpandAdvanced_: function(route) {
      return (
                 this.tagName == 'SETTINGS-BASIC-PAGE'
                 // <if expr="chromeos">
                 || this.tagName == 'OS-SETTINGS-PAGE'
                 // </if>
                 ) &&
          settings.routes.ADVANCED && settings.routes.ADVANCED.contains(route);
    },

    /**
     * Finds the settings section corresponding to the given route. If the
     * section is lazily loaded it force-renders it.
     * Note: If the section resides within "advanced" settings, a
     * 'hide-container' event is fired (necessary to avoid flashing). Callers
     * are responsible for firing a 'show-container' event.
     * @param {!settings.Route} route
     * @return {!Promise<!SettingsSectionElement>}
     * @private
     */
    ensureSectionForRoute_: function(route) {
      const section = this.getSection(route.section);
      if (section != null) {
        return Promise.resolve(section);
      }

      // The function to use to wait for <dom-if>s to render.
      const waitFn = Polymer.RenderStatus.beforeNextRender.bind(null, this);

      return new Promise(resolve => {
        if (this.shouldExpandAdvanced_(route)) {
          this.fire('hide-container');
          waitFn(() => {
            this.$$('#advancedPageTemplate').get().then(() => {
              resolve(this.getSection(route.section));
            });
          });
        } else {
          waitFn(() => {
            resolve(this.getSection(route.section));
          });
        }
      });
    },

    /**
     * @param {!settings.Route} route
     * @private
     */
    enterSubpage_: function(route) {
      this.lastScrollTop_ = this.scroller.scrollTop;
      this.scroller.scrollTop = 0;
      this.classList.add('showing-subpage');
      this.fire('subpage-expand');
      this.ensureSectionForRoute_(route).then(section => {
        section.classList.add('expanded');
        // Fire event used by a11y tests only.
        this.fire('settings-section-expanded');

        this.fire('show-container');
      });
    },

    /**
     * @param {!settings.Route} oldRoute
     * @return {!Promise<void>}
     * @private
     */
    enterMainPage_: function(oldRoute) {
      const oldSection = this.getSection(oldRoute.section);
      oldSection.classList.remove('expanded');
      this.classList.remove('showing-subpage');
      return new Promise((res, rej) => {
        requestAnimationFrame(() => {
          if (settings.lastRouteChangeWasPopstate()) {
            this.scroller.scrollTop = this.lastScrollTop_;
          }
          this.fire('showing-main-page');
          res();
        });
      });
    },

    /**
     * @param {!settings.Route} route
     * @private
     */
    scrollToSection_: function(route) {
      this.ensureSectionForRoute_(route).then(section => {
        if (!this.inSearchMode) {
          this.fire('showing-section', section);
        }
        this.fire('show-container');
      });
    },

    /**
     * Detects which state transition is appropriate for the given new/old
     * routes.
     * @param {!settings.Route} newRoute
     * @param {settings.Route} oldRoute
     * @private
     */
    getStateTransition_(newRoute, oldRoute) {
      const containsNew = this.containsRoute(newRoute);
      const containsOld = this.containsRoute(oldRoute);

      if (!containsNew && !containsOld) {
        // Nothing to do, since none of the old/new routes belong to this page.
        return null;
      }

      // Case where going from |this| page to an unrelated page. For example:
      //  |this| is settings-basic-page AND
      //  oldRoute is /searchEngines AND
      //  newRoute is /help.
      if (containsOld && !containsNew) {
        return [classifyRoute(oldRoute), RouteState.TOP_LEVEL];
      }

      // Case where return from an unrelated page to |this| page. For example:
      //  |this| is settings-basic-page AND
      //  oldRoute is /help AND
      //  newRoute is /searchEngines
      if (!containsOld && containsNew) {
        return [RouteState.TOP_LEVEL, classifyRoute(newRoute)];
      }

      // Case where transitioning between routes that both belong to |this|
      // page.
      return [classifyRoute(oldRoute), classifyRoute(newRoute)];
    },

    /**
     * @param {!settings.Route} newRoute
     * @param {settings.Route} oldRoute
     */
    currentRouteChanged(newRoute, oldRoute) {
      const transition = this.getStateTransition_(newRoute, oldRoute);
      if (transition === null) {
        return;
      }

      const oldState = transition[0];
      const newState = transition[1];
      assert(this.validTransitions_.get(oldState).has(newState));

      if (oldState == RouteState.TOP_LEVEL) {
        if (newState == RouteState.SECTION) {
          this.scrollToSection_(newRoute);
        } else if (newState == RouteState.SUBPAGE) {
          this.enterSubpage_(newRoute);
        }
        // Nothing to do here for the case of RouteState.DIALOG or TOP_LEVEL.
        // The latter happens when navigating from '/?search=foo' to '/'
        // (clearing search results).
        return;
      }

      if (oldState == RouteState.SECTION) {
        if (newState == RouteState.SECTION) {
          this.scrollToSection_(newRoute);
        } else if (newState == RouteState.SUBPAGE) {
          this.enterSubpage_(newRoute);
        } else if (newState == RouteState.TOP_LEVEL) {
          this.scroller.scrollTop = 0;
        }
        // Nothing to do here for the case of RouteState.DIALOG.
        return;
      }

      if (oldState == RouteState.SUBPAGE) {
        if (newState == RouteState.SECTION) {
          this.enterMainPage_(oldRoute);

          // Scroll to the corresponding section, only if the user explicitly
          // navigated to a section (via the menu).
          if (!settings.lastRouteChangeWasPopstate()) {
            this.scrollToSection_(newRoute);
          }
        } else if (newState == RouteState.SUBPAGE) {
          // Handle case where the two subpages belong to
          // different sections, but are linked to each other. For example
          // /storage and /accounts (in ChromeOS).
          if (!oldRoute.contains(newRoute) && !newRoute.contains(oldRoute)) {
            this.enterMainPage_(oldRoute).then(() => {
              this.enterSubpage_(newRoute);
            });
            return;
          }

          // Handle case of subpage to sub-subpage navigation.
          if (oldRoute.contains(newRoute)) {
            this.scroller.scrollTop = 0;
            return;
          }
          // When going from a sub-subpage to its parent subpage, scroll
          // position is automatically restored, because we focus the
          // sub-subpage entry point.
        } else if (newState == RouteState.TOP_LEVEL) {
          this.enterMainPage_(oldRoute);
        } else if (newState == RouteState.DIALOG) {
          // The only known case currently for such a transition is from
          // /storage to /clearBrowserData.
          this.enterMainPage_(oldRoute);
        }
        return;
      }

      if (oldState == RouteState.INITIAL) {
        if (newState == RouteState.SECTION) {
          this.scrollToSection_(newRoute);
        } else if (newState == RouteState.SUBPAGE) {
          this.enterSubpage_(newRoute);
        }
        // Nothing to do here for the case of RouteState.DIALOG and TOP_LEVEL.
        return;
      }

      if (oldState == RouteState.DIALOG) {
        if (newState == RouteState.SUBPAGE) {
          // The only known case currently for such a transition is from
          // /clearBrowserData back to /storage.
          this.enterSubpage_(newRoute);
        }
        // Nothing to do for all other cases.
      }
    },

    /**
     * TODO(dpapad): Rename this to |querySection| to distinguish it from
     * ensureSectionForRoute_() which force-renders the section as needed.
     * Helper function to get a section from the local DOM.
     * @param {string} section Section name of the element to get.
     * @return {?SettingsSectionElement}
     */
    getSection: function(section) {
      if (!section) {
        return null;
      }
      return /** @type {?SettingsSectionElement} */ (
          this.$$(`settings-section[section="${section}"]`));
    },
  };

  return {MainPageBehavior: MainPageBehavior};
});
