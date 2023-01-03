// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from 'chrome://resources/js/assert_ts.js';
import {beforeNextRender, dedupingMixin, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from './assert_extras.js';
import {Constructor} from './common/types.js';
import {ensureLazyLoaded} from './ensure_lazy_loaded.js';
import {SettingsIdleLoadElement} from './os_settings_page/settings_idle_load.js';
import {RouteObserverMixin, RouteObserverMixinInterface} from './route_observer_mixin.js';
import {MinimumRoutes, Route, Router} from './router.js';

/**
 * A categorization of every possible Settings URL, necessary for implementing
 * a finite state machine.
 */
enum RouteState {
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

function classifyRoute(route: Route|undefined): RouteState {
  if (!route) {
    return RouteState.INITIAL;
  }
  const routes = Router.getInstance().getRoutes() as MinimumRoutes;
  if (route === routes.BASIC || route === routes.ABOUT) {
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

const ALL_STATES = new Set([
  RouteState.DIALOG,
  RouteState.SECTION,
  RouteState.SUBPAGE,
  RouteState.TOP_LEVEL,
]);

/**
 * A map holding all valid state transitions.
 */
const VALID_TRANSITIONS = new Map([
  [RouteState.INITIAL, ALL_STATES],
  [
    RouteState.DIALOG,
    new Set([
      RouteState.SECTION,
      RouteState.SUBPAGE,
      RouteState.TOP_LEVEL,
    ]),
  ],
  [RouteState.SECTION, ALL_STATES],
  [RouteState.SUBPAGE, ALL_STATES],
  [RouteState.TOP_LEVEL, ALL_STATES],
]);

export interface MainPageMixinInterface extends RouteObserverMixinInterface {
  containsRoute(route: Route|undefined): boolean;
  querySection(section: string): HTMLElement|null;
  loadAdvancedPage(): Promise<Element>;
}

/**
 * Responds to route changes by expanding, collapsing, or scrolling to
 * sections on the page. Expanded sections take up the full height of the
 * container. At most one section should be expanded at any given time.
 */
export const MainPageMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<MainPageMixinInterface> => {
      const superclassBase = RouteObserverMixin(superClass);

      class MainPageMixinInternal extends superclassBase implements
          MainPageMixinInterface {
        private lastScrollTop_: number = 0;

        private get scroller_(): HTMLElement {
          const hostEl = (this.getRootNode() as ShadowRoot).host;
          return castExists(hostEl ? hostEl.parentElement : document.body);
        }

        /**
         * Method to be overridden by users of MainPageMixin.
         * @return Whether the given route is part of |this| page.
         */
        containsRoute(_route: Route|undefined): boolean {
          assertNotReached();
        }

        private shouldExpandAdvanced_(route: Route): boolean {
          const routes = Router.getInstance().getRoutes() as MinimumRoutes;
          return (this.tagName === 'OS-SETTINGS-PAGE') && routes.ADVANCED &&
              routes.ADVANCED.contains(route);
        }

        loadAdvancedPage(): Promise<Element> {
          return this.shadowRoot!
              .querySelector<SettingsIdleLoadElement>(
                  '#advancedPageTemplate')!.get();
        }

        /**
         * Finds the settings section corresponding to the given route. If the
         * section is lazily loaded it force-renders it.
         * Note: If the section resides within "advanced" settings, a
         * 'hide-container' event is fired (necessary to avoid flashing).
         * Callers are responsible for firing a 'show-container' event.
         */
        private ensureSectionForRoute_(route: Route): Promise<HTMLElement> {
          const section = this.querySection(route.section);
          if (section) {
            return Promise.resolve(section);
          }

          // The function to use to wait for <dom-if>s to render.
          const waitFn = beforeNextRender.bind(null, this);

          return new Promise(resolve => {
            if (this.shouldExpandAdvanced_(route)) {
              this.dispatchCustomEvent_('hide-container');
              waitFn(async () => {
                await this.loadAdvancedPage();
                resolve(castExists(this.querySection(route.section)));
              });
            } else {
              waitFn(() => {
                resolve(castExists(this.querySection(route.section)));
              });
            }
          });
        }

        private async enterSubpage_(route: Route) {
          this.lastScrollTop_ = this.scroller_.scrollTop;
          this.scroller_.scrollTop = 0;
          this.classList.add('showing-subpage');
          this.dispatchCustomEvent_('subpage-expand');

          // Explicitly load the lazy_load module, since all subpages reside in
          // the lazy loaded module.
          ensureLazyLoaded();

          const section = await this.ensureSectionForRoute_(route);
          section.classList.add('expanded');
          // Fire event used by a11y tests only.
          this.dispatchCustomEvent_('settings-section-expanded');
          this.dispatchCustomEvent_('show-container');
        }

        private enterMainPage_(oldRoute: Route): Promise<void> {
          const oldSection = castExists(this.querySection(oldRoute.section));
          oldSection.classList.remove('expanded');
          this.classList.remove('showing-subpage');
          return new Promise((resolve) => {
            requestAnimationFrame(() => {
              if (Router.getInstance().lastRouteChangeWasPopstate()) {
                this.scroller_.scrollTop = this.lastScrollTop_;
              }
              this.dispatchCustomEvent_('showing-main-page');
              resolve();
            });
          });
        }

        private async scrollToSection_(route: Route) {
          const section = await this.ensureSectionForRoute_(route);
          this.dispatchCustomEvent_('showing-section', {detail: section});
          this.dispatchCustomEvent_('show-container');
        }

        /**
         * Detects which state transition is appropriate for the given new/old
         * routes.
         */
        private getStateTransition_(newRoute: Route, oldRoute?: Route) {
          const containsNew = this.containsRoute(newRoute);
          const containsOld = this.containsRoute(oldRoute);

          if (!containsNew && !containsOld) {
            // Nothing to do, since none of the old/new routes belong to this
            // page.
            return null;
          }

          // Case where going from |this| page to an unrelated page.
          // For example:
          //  |this| is os-settings-page AND
          //  oldRoute is /searchEngines AND
          //  newRoute is /help.
          if (containsOld && !containsNew) {
            return [classifyRoute(oldRoute), RouteState.TOP_LEVEL];
          }

          // Case where return from an unrelated page to |this| page.
          // For example:
          //  |this| is os-settings-page AND
          //  oldRoute is /help AND
          //  newRoute is /searchEngines
          if (!containsOld && containsNew) {
            return [RouteState.TOP_LEVEL, classifyRoute(newRoute)];
          }

          // Case where transitioning between routes that both belong to |this|
          // page.
          return [classifyRoute(oldRoute), classifyRoute(newRoute)];
        }

        override currentRouteChanged(newRoute: Route, oldRoute?: Route) {
          const transition = this.getStateTransition_(newRoute, oldRoute);
          if (transition === null) {
            return;
          }

          const oldState = transition[0];
          const newState = transition[1];
          assert(VALID_TRANSITIONS.get(oldState)!.has(newState));

          if (oldState === RouteState.TOP_LEVEL) {
            if (newState === RouteState.SECTION) {
              this.scrollToSection_(newRoute);
            } else if (newState === RouteState.SUBPAGE) {
              this.enterSubpage_(newRoute);
            }
            // Nothing to do here for the case of RouteState.DIALOG or
            // TOP_LEVEL. The latter happens when navigating from '/?search=foo'
            // to '/' (clearing search results).
            return;
          }

          if (oldState === RouteState.SECTION) {
            if (newState === RouteState.SECTION) {
              this.scrollToSection_(newRoute);
            } else if (newState === RouteState.SUBPAGE) {
              this.enterSubpage_(newRoute);
            } else if (newState === RouteState.TOP_LEVEL) {
              this.scroller_.scrollTop = 0;
            }
            // Nothing to do here for the case of RouteState.DIALOG.
            return;
          }

          if (oldState === RouteState.SUBPAGE) {
            assert(oldRoute);
            if (newState === RouteState.SECTION) {
              this.enterMainPage_(oldRoute);

              // Scroll to the corresponding section, only if the user
              // explicitly navigated to a section (via the menu).
              if (!Router.getInstance().lastRouteChangeWasPopstate()) {
                this.scrollToSection_(newRoute);
              }
            } else if (newState === RouteState.SUBPAGE) {
              // Handle case where the two subpages belong to
              // different sections, but are linked to each other. For example
              // /storage and /accounts (in ChromeOS).
              if (!oldRoute.contains(newRoute) &&
                  !newRoute.contains(oldRoute)) {
                this.enterMainPage_(oldRoute).then(() => {
                  this.enterSubpage_(newRoute);
                });
                return;
              }

              // Handle case of subpage to sub-subpage navigation.
              if (oldRoute.contains(newRoute)) {
                this.scroller_.scrollTop = 0;
                return;
              }
              // When going from a sub-subpage to its parent subpage, scroll
              // position is automatically restored, because we focus the
              // sub-subpage entry point.
            } else if (newState === RouteState.TOP_LEVEL) {
              this.enterMainPage_(oldRoute);
            } else if (newState === RouteState.DIALOG) {
              // The only known case currently for such a transition is from
              // /storage to /clearBrowserData.
              this.enterMainPage_(oldRoute);
            }
            return;
          }

          if (oldState === RouteState.INITIAL) {
            if (newState === RouteState.SECTION) {
              this.scrollToSection_(newRoute);
            } else if (newState === RouteState.SUBPAGE) {
              this.enterSubpage_(newRoute);
            }
            // Nothing to do here for the case of RouteState.DIALOG and
            // TOP_LEVEL.
            return;
          }

          if (oldState === RouteState.DIALOG) {
            if (newState === RouteState.SUBPAGE) {
              // The only known case currently for such a transition is from
              // /clearBrowserData back to /storage.
              this.enterSubpage_(newRoute);
            }
            // Nothing to do for all other cases.
          }
        }

        /**
         * Helper function to get a section from the local DOM.
         */
        querySection(section: string): HTMLElement|null {
          if (!section) {
            return null;
          }
          return this.shadowRoot!.querySelector(
              `os-settings-section[section="${section}"]`);
        }

        private dispatchCustomEvent_(
            name: string, options?: CustomEventInit<unknown>) {
          const event = new CustomEvent(
              name, {bubbles: true, composed: true, ...options});
          this.dispatchEvent(event);
        }
      }

      return MainPageMixinInternal;
    });
