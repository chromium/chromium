// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {assert} from 'chrome://resources/js/assert.js';
import type { PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {beforeNextRender, dedupingMixin} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BaseMixin} from '../base_mixin.js';
import type {SettingsIdleLoadElement} from '../controls/settings_idle_load.js';
import {ensureLazyLoaded} from '../ensure_lazy_loaded.js';
import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';
import type {Route} from '../router.js';
import { Router} from '../router.js';
// clang-format on

/**
 * A categorization of every possible Settings URL, necessary for implementing
 * a finite state machine.
 */
export enum RouteState {
  // Initial state before anything has loaded yet.
  INITIAL = 'initial',
  // A dialog that has a dedicated URL (e.g. /importData).
  DIALOG = 'dialog',
  // A section (basically a scroll position within the top level page, e.g,
  // /appearance.
  SECTION = 'section',
  // A subpage, or sub-subpage e.g, /searchEngins.
  SUBPAGE = 'subpage',
  // The top level Settings page, '/'.
  TOP_LEVEL = 'top-level',
}

let guestTopLevelRoute = routes.SEARCH;
// <if expr="chromeos_ash">
guestTopLevelRoute = routes.PRIVACY;
// </if>

const TOP_LEVEL_EQUIVALENT_ROUTE: Route =
    loadTimeData.getBoolean('isGuest') ? guestTopLevelRoute : routes.PEOPLE;

function classifyRoute(route: Route|null): RouteState {
  if (!route) {
    return RouteState.INITIAL;
  }
  const routes = Router.getInstance().getRoutes();
  if (route === routes.BASIC) {
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

type Constructor<T> = new (...args: any[]) => T;

/**
 * Responds to route changes by expanding, collapsing, or scrolling to
 * sections on the page. Expanded sections take up the full height of the
 * container. At most one section should be expanded at any given time.
 */
export const MainPageMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<MainPageMixinInterface> => {
      const superClassBase = BaseMixin(superClass);

      class MainPageMixin extends superClassBase {
        scroller: HTMLElement|null = null;
        private validTransitions_: Map<RouteState, Set<RouteState>>;
        private lastScrollTop_: number = 0;

        // Populated by Polymer itself.
        domHost: HTMLElement|null;

        constructor(...args: any[]) {
          super(...args);

          /**
           * A map holding all valid state transitions.
           */
          this.validTransitions_ = (function() {
            const allStates = new Set([
              RouteState.DIALOG,
              RouteState.SECTION,
              RouteState.SUBPAGE,
              RouteState.TOP_LEVEL,
            ]);

            return new Map([
              [RouteState.INITIAL, allStates],
              [
                RouteState.DIALOG,
                new Set([
                  RouteState.SECTION,
                  RouteState.SUBPAGE,
                  RouteState.TOP_LEVEL,
                ]),
              ],
              [RouteState.SECTION, allStates],
              [RouteState.SUBPAGE, allStates],
              [RouteState.TOP_LEVEL, allStates],
            ]);
          })();
        }

        override connectedCallback() {
          this.scroller =
              this.domHost ? this.domHost.parentElement : document.body;

          // Purposefully calling this after |scroller| has been initialized.
          super.connectedCallback();
        }

        /**
         * Method to be overridden by users of MainPageMixin.
         * @return Whether the given route is part of |this| page.
         */
        containsRoute(_route: Route|null): boolean {
          return false;
        }

        private shouldExpandAdvanced_(route: Route): boolean {
          const routes = Router.getInstance().getRoutes();
          return this.tagName === 'SETTINGS-BASIC-PAGE' && !!routes.ADVANCED &&
              routes.ADVANCED.contains(route);
        }

        /**
         * Finds the settings section corresponding to the given route. If the
         * section is lazily loaded it force-renders it.
         * Note: If the section resides within "advanced" settings, a
         * 'hide-container' event is fired (necessary to avoid flashing).
         * Callers are responsible for firing a 'show-container' event.
         */
        private ensureSectionForRoute_(route: Route): Promise<HTMLElement> {
          const section = this.getSection(route.section);
          if (section !== null) {
            return Promise.resolve(section);
          }

          // The function to use to wait for <dom-if>s to render.
          const waitFn = beforeNextRender.bind(null, this);

          return new Promise<HTMLElement>(resolve => {
            if (this.shouldExpandAdvanced_(route)) {
              this.fire('hide-container');
              waitFn(() => {
                this.$$<SettingsIdleLoadElement>('#advancedPageTemplate')!.get()
                    .then(() => {
                      resolve(this.getSection(route.section)!);
                    });
              });
            } else {
              waitFn(() => {
                resolve(this.getSection(route.section)!);
              });
            }
          });
        }

        /**
         * Finds the settings-section instances corresponding to the given
         * route. If the section is lazily loaded it force-renders it. Note: If
         * the section resides within "advanced" settings, a 'hide-container'
         * event is fired (necessary to avoid flashing). Callers are responsible
         * for firing a 'show-container' event.
         */
        private ensureSectionsForRoute_(route: Route): Promise<HTMLElement[]> {
          const sections = this.querySettingsSections_(route.section);
          if (sections.length > 0) {
            return Promise.resolve(sections);
          }

          // The function to use to wait for <dom-if>s to render.
          const waitFn = beforeNextRender.bind(null, this);

          return new Promise(resolve => {
            if (this.shouldExpandAdvanced_(route)) {
              this.fire('hide-container');
              waitFn(() => {
                this.$$<SettingsIdleLoadElement>('#advancedPageTemplate')!.get()
                    .then(() => {
                      resolve(this.querySettingsSections_(route.section));
                    });
              });
            } else {
              waitFn(() => {
                resolve(this.querySettingsSections_(route.section));
              });
            }
          });
        }

        private enterSubpage_(route: Route) {
          this.lastScrollTop_ = this.scroller!.scrollTop;
          this.scroller!.scrollTop = 0;
          this.classList.add('showing-subpage');
          this.fire('subpage-expand');

          // Explicitly load the lazy_load.html module, since all subpages
          // reside in the lazy loaded module.
          ensureLazyLoaded();

          this.ensureSectionForRoute_(route).then(section => {
            section.classList.add('expanded');
            // Fire event used by a11y tests only.
            this.fire('settings-section-expanded');

            this.fire('show-container');
          });
        }

        private enterMainPage_(oldRoute: Route): Promise<void> {
          const oldSection = this.getSection(oldRoute.section)!;
          oldSection.classList.remove('expanded');
          this.classList.remove('showing-subpage');
          return new Promise((res) => {
            requestAnimationFrame(() => {
              if (Router.getInstance().lastRouteChangeWasPopstate()) {
                this.scroller!.scrollTop = this.lastScrollTop_;
              }
              this.fire('showing-main-page');
              res();
            });
          });
        }

        /**
         * Shows the section(s) corresponding to |newRoute| and hides the
         * previously |active| section(s), if any.
         */
        private switchToSections_(newRoute: Route) {
          this.ensureSectionsForRoute_(newRoute).then(sections => {
            // Clear any previously |active| section.
            const oldSections =
                this.shadowRoot!.querySelectorAll(`settings-section[active]`);
            for (const s of oldSections) {
              s.toggleAttribute('active', false);
            }

            for (const s of sections) {
              s.toggleAttribute('active', true);
            }

            this.fire('show-container');
          });
        }

        /**
         * Detects which state transition is appropriate for the given new/old
         * routes.
         */
        private getStateTransition_(newRoute: Route, oldRoute: Route|null) {
          const containsNew = this.containsRoute(newRoute);
          const containsOld = this.containsRoute(oldRoute);

          if (!containsNew && !containsOld) {
            // Nothing to do, since none of the old/new routes belong to this
            // page.
            return null;
          }

          // Case where going from |this| page to an unrelated page. For
          // example:
          //  |this| is settings-basic-page AND
          //  oldRoute is /searchEngines AND
          //  newRoute is /help.
          if (containsOld && !containsNew) {
            return [classifyRoute(oldRoute), RouteState.TOP_LEVEL];
          }

          // Case where return from an unrelated page to |this| page. For
          // example:
          //  |this| is settings-basic-page AND
          //  oldRoute is /help AND
          //  newRoute is /searchEngines
          if (!containsOld && containsNew) {
            return [RouteState.TOP_LEVEL, classifyRoute(newRoute)];
          }

          // Case where transitioning between routes that both belong to |this|
          // page.
          return [classifyRoute(oldRoute), classifyRoute(newRoute)];
        }

        // TODO(dpapad): Figure out why adding the |override| keyword here
        // throws an error.
        currentRouteChanged(newRoute: Route, oldRoute: Route|null) {
          const transition = this.getStateTransition_(newRoute, oldRoute);
          if (transition === null) {
            return;
          }

          const oldState = transition[0];
          const newState = transition[1];
          assert(this.validTransitions_.get(oldState)!.has(newState));

          if (oldState === RouteState.TOP_LEVEL) {
            if (newState === RouteState.SECTION) {
              this.switchToSections_(newRoute);
            } else if (newState === RouteState.SUBPAGE) {
              this.switchToSections_(newRoute);
              this.enterSubpage_(newRoute);
            } else if (newState === RouteState.TOP_LEVEL) {
              // Case when navigating from '/?search=foo' to '/' (clearing
              // search results).
              this.switchToSections_(TOP_LEVEL_EQUIVALENT_ROUTE);
            } else if (newState === RouteState.DIALOG) {
              // Case when user clicks "Reset all settings" from within the
              // settings-reset-profile-banner to navigate to
              // /resetProfileSettings.
              this.switchToSections_(newRoute);
            }
            return;
          }

          if (oldState === RouteState.SECTION) {
            if (newState === RouteState.SECTION) {
              this.switchToSections_(newRoute);
            } else if (newState === RouteState.SUBPAGE) {
              this.switchToSections_(newRoute);
              this.enterSubpage_(newRoute);
            } else if (newState === RouteState.TOP_LEVEL) {
              this.switchToSections_(TOP_LEVEL_EQUIVALENT_ROUTE);
              this.scroller!.scrollTop = 0;
            }
            // Nothing to do here for the case of RouteState.DIALOG.
            return;
          }

          if (oldState === RouteState.SUBPAGE) {
            if (newState === RouteState.SECTION) {
              this.enterMainPage_(oldRoute!);
              this.switchToSections_(newRoute);
            } else if (newState === RouteState.SUBPAGE) {
              // Handle case where the two subpages belong to
              // different sections, but are linked to each other. For example
              // /storage and /accounts (in ChromeOS).
              if (!oldRoute!.contains(newRoute) &&
                  !newRoute.contains(oldRoute!)) {
                this.enterMainPage_(oldRoute!).then(() => {
                  this.switchToSections_(newRoute);
                  this.enterSubpage_(newRoute);
                });
                return;
              }

              // Handle case of subpage to sub-subpage navigation.
              if (oldRoute!.contains(newRoute)) {
                this.scroller!.scrollTop = 0;
                return;
              }
              // When going from a sub-subpage to its parent subpage, scroll
              // position is automatically restored, because we focus the
              // sub-subpage entry point.
            } else if (newState === RouteState.TOP_LEVEL) {
              this.enterMainPage_(oldRoute!);
            } else if (newState === RouteState.DIALOG) {
              // The only known cases currently for such a transition are from
              // 1) /synceSetup to /signOut
              // 2) /synceSetup to /clearBrowserData using the "back" arrow
              this.enterMainPage_(oldRoute!);
              this.switchToSections_(newRoute);
            }
            return;
          }

          if (oldState === RouteState.INITIAL) {
            if ([RouteState.SECTION, RouteState.DIALOG].includes(newState)) {
              this.switchToSections_(newRoute);
            } else if (newState === RouteState.SUBPAGE) {
              this.switchToSections_(newRoute);
              this.enterSubpage_(newRoute);
            } else if (newState === RouteState.TOP_LEVEL) {
              this.switchToSections_(TOP_LEVEL_EQUIVALENT_ROUTE);
            }
            return;
          }

          if (oldState === RouteState.DIALOG) {
            if (newState === RouteState.SUBPAGE) {
              // The only known cases currently for such a transition are from
              // 1) /signOut to /syncSetup
              // 2) /clearBrowserData to /syncSetup
              this.switchToSections_(newRoute);
              this.enterSubpage_(newRoute);
            }
            // Nothing to do for all other cases.
          }
        }

        /**
         * TODO(dpapad): Rename this to |querySection| to distinguish it from
         * ensureSectionForRoute_() which force-renders the section as needed.
         * Helper function to get a section from the local DOM.
         * @param section Section name of the element to get.
         */
        getSection(section: string): HTMLElement|null {
          if (!section) {
            return null;
          }
          return this.$$(`settings-section[section="${section}"]`);
        }

        /*
         * @param sectionName Section name of the element to get.
         */
        private querySettingsSections_(sectionName: string): HTMLElement[] {
          const result = [];
          const section = this.getSection(sectionName);

          if (section) {
            result.push(section);
          }

          const extraSections = this.shadowRoot!.querySelectorAll<HTMLElement>(
              `settings-section[nest-under-section="${sectionName}"]`);
          if (extraSections.length > 0) {
            result.push(...extraSections);
          }
          return result;
        }
      }

      return MainPageMixin;
    });

export interface MainPageMixinInterface {
  scroller: HTMLElement|null;
  containsRoute(route: Route|null): boolean;
}
