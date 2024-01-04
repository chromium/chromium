// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Responds to route changes by "activating" the respective top-level page,
 * effectively making that page visible to the user and potentially hiding other
 * pages.
 */

import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {beforeNextRender, dedupingMixin, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from '../assert_extras.js';
import {isRevampWayfindingEnabled} from '../common/load_time_booleans.js';
import {RouteObserverMixin, RouteObserverMixinInterface} from '../common/route_observer_mixin.js';
import {Constructor} from '../common/types.js';
import {ensureLazyLoaded} from '../ensure_lazy_loaded.js';
import {Section} from '../mojom-webui/routes.mojom-webui.js';
import {SettingsIdleLoadElement} from '../os_settings_page/settings_idle_load.js';
import {isAboutRoute, isAdvancedRoute, Route, Router, routes} from '../router.js';

import {PageDisplayerElement} from './page_displayer.js';

/**
 * A categorization of every possible Settings URL, necessary for implementing
 * a finite state machine.
 */
enum RouteState {
  // Initial state before anything has loaded yet.
  INITIAL = 'initial',
  // The root Settings page, '/'.
  ROOT = 'root',
  // A section, basically a scroll position within the root page.
  // After infinite scroll is removed, this is a top-level page.
  // e.g. /network, /bluetooth, /device
  SECTION = 'section',
  // A subpage, or nested subpage, e.g. /networkDetail.
  SUBPAGE = 'subpage',
  // A navigable dialog that has a dedicated URL. Currently unused in Settings.
  DIALOG = 'dialog',
}

function classifyRoute(route: Route|undefined): RouteState {
  if (!route) {
    return RouteState.INITIAL;
  }
  if (route === routes.BASIC) {
    return RouteState.ROOT;
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
  RouteState.ROOT,
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
      RouteState.ROOT,
    ]),
  ],
  [RouteState.SECTION, ALL_STATES],
  [RouteState.SUBPAGE, ALL_STATES],
  [RouteState.ROOT, ALL_STATES],
]);

/**
 * The route for the first page listed in the Settings menu.
 */
const FIRST_PAGE_ROUTE: Route = routes.INTERNET;

export interface MainPageMixinInterface extends RouteObserverMixinInterface {
  containsRoute(route: Route|undefined): boolean;
  loadAdvancedPage(): Promise<Element>;
}

export const MainPageMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<MainPageMixinInterface> => {
      const superclassBase = RouteObserverMixin(superClass);

      class MainPageMixinInternal extends superclassBase implements
          MainPageMixinInterface {
        private lastScrollTop_: number = 0;

        /**
         * The scroller is derived from the #container ancestor element.
         */
        private get scroller_(): HTMLElement {
          const hostEl = (this.getRootNode() as ShadowRoot).host;
          return castExists(
              hostEl ? hostEl.closest('#container') : document.body);
        }

        /**
         * Method to be overridden by users of MainPageMixin.
         * @return Whether the given route is part of |this| page.
         */
        containsRoute(_route: Route|undefined): boolean {
          assertNotReached();
        }

        loadAdvancedPage(): Promise<Element> {
          return this.shadowRoot!
              .querySelector<SettingsIdleLoadElement>(
                  '#advancedPageTemplate')!.get();
        }

        private async enterSubpage(route: Route): Promise<void> {
          // Immediately record the last scroll position before continuing.
          this.lastScrollTop_ = this.scroller_.scrollTop;

          // Make the parent page visible to ensure the subpage is visible
          await this.activatePage(route);
          this.scroller_.scrollTop = 0;
          this.classList.add('showing-subpage');
          this.dispatchCustomEvent_('showing-subpage');

          // Explicitly load the lazy_load module, since all subpages reside in
          // the lazy loaded module.
          ensureLazyLoaded();

          this.dispatchCustomEvent_('show-container');
        }

        /**
         * Indicates the page transition of leaving a subpage and entering the
         * main page by emitting a `showing-main-page` event.
         * If the page transition was a pop state (e.g. clicking back button on
         * a subpage), then the cached scroll position on the main page is
         * restored.
         */
        private enterMainPage(): Promise<void> {
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

        /**
         * Simple helper method to display a page/section depending on if the
         * `OsSettingsRevampWayfinding` is enabled.
         */
        private showPage(route: Route): void {
          if (isRevampWayfindingEnabled()) {
            this.activatePage(route, {focus: true});
          } else {
            this.scrollToSection(route);
          }
        }

        private async scrollToSection(route: Route): Promise<void> {
          const page = await this.ensurePageForRoute(route);
          this.dispatchCustomEvent_('showing-section', {detail: page});
          this.dispatchCustomEvent_('show-container');
        }

        /**
         * Effectively displays the page for the given |route| by marking the
         * respective page-displayer element as active, and hides all other
         * pages by marking them as inactive. Also, optionally transfers focus
         * to the page content.
         */
        private async activatePage(route: Route, options: {
          focus?: boolean,
        } = {}): Promise<void> {
          const page = await this.ensurePageForRoute(route);

          const previouslyActive =
              this.shadowRoot!.querySelectorAll<PageDisplayerElement>(
                  'page-displayer[active]');
          for (const prevPage of previouslyActive) {
            prevPage.active = false;
          }

          page.active = true;
          if (options.focus) {
            page.focus();
          }

          this.dispatchCustomEvent_('show-container');
        }

        /**
         * Activate and display the first page (Network page). This page
         * should be the default visible page when the root page is visited.
         */
        private activateInitialPage(): void {
          if (isRevampWayfindingEnabled()) {
            // Note: This should not focus the Network page since the search box
            // should be the element initially focused after app load.
            this.activatePage(FIRST_PAGE_ROUTE, {focus: false});
          }
        }

        /**
         * Detects which state transition is appropriate for the given new/old
         * routes.
         */
        private getStateTransition_(newRoute: Route, oldRoute?: Route):
            [RouteState, RouteState]|null {
          const containsNew = this.containsRoute(newRoute);
          const containsOld = this.containsRoute(oldRoute);

          if (!containsNew && !containsOld) {
            // Nothing to do, since none of the old/new routes belong to this
            // page.
            return null;
          }

          // Case where going from |this| page to an unrelated page.
          // For example:
          //  |this| is main-page-container AND
          //  oldRoute is /searchEngines AND
          //  newRoute is /help.
          if (containsOld && !containsNew) {
            return [classifyRoute(oldRoute), RouteState.ROOT];
          }

          // Case where return from an unrelated page to |this| page.
          // For example:
          //  |this| is main-page-container AND
          //  oldRoute is /help AND
          //  newRoute is /searchEngines
          if (!containsOld && containsNew) {
            return [RouteState.ROOT, classifyRoute(newRoute)];
          }

          // Case where transitioning between routes that both belong to |this|
          // page.
          return [classifyRoute(oldRoute), classifyRoute(newRoute)];
        }

        override currentRouteChanged(newRoute: Route, oldRoute?: Route): void {
          const transition = this.getStateTransition_(newRoute, oldRoute);
          if (transition === null) {
            return;
          }

          const [oldState, newState] = transition;
          assert(VALID_TRANSITIONS.get(oldState)!.has(newState));

          if (oldState === RouteState.INITIAL) {
            switch (newState) {
              case RouteState.SECTION:
                this.showPage(newRoute);
                return;

              case RouteState.SUBPAGE:
                this.enterSubpage(newRoute);
                return;

              case RouteState.ROOT:
                this.activateInitialPage();
                return;

              // Nothing to do here for the DIALOG case.
              case RouteState.DIALOG:
              default:
                return;
            }
          }

          if (oldState === RouteState.ROOT) {
            switch (newState) {
              case RouteState.SECTION:
                this.showPage(newRoute);
                return;

              // Navigating directly to a subpage via search on the main page
              case RouteState.SUBPAGE:
                this.enterSubpage(newRoute);
                return;

              // Happens when clearing search results (Navigating from
              // '/?search=foo' to '/')
              case RouteState.ROOT:
                this.activateInitialPage();
                return;

              // Nothing to do here for the DIALOG case.
              case RouteState.DIALOG:
              default:
                return;
            }
          }

          if (oldState === RouteState.SECTION) {
            switch (newState) {
              case RouteState.SECTION:
                this.showPage(newRoute);
                return;

              case RouteState.SUBPAGE:
                this.enterSubpage(newRoute);
                return;

              case RouteState.ROOT:
                this.scroller_.scrollTop = 0;
                this.activateInitialPage();
                return;

              // Nothing to do here for the case of DIALOG.
              case RouteState.DIALOG:
              default:
                return;
            }
          }

          if (oldState === RouteState.SUBPAGE) {
            assert(oldRoute);
            switch (newState) {
              case RouteState.SECTION:
                if (isRevampWayfindingEnabled()) {
                  this.enterMainPage().then(() => {
                    this.activatePage(newRoute, {focus: true});
                  });
                } else {
                  this.enterMainPage();

                  // Only if the user explicitly navigated to a section (via
                  // the left menu), scroll to the corresponding section.
                  if (!Router.getInstance().lastRouteChangeWasPopstate()) {
                    this.scrollToSection(newRoute);
                  }
                }
                return;

              case RouteState.SUBPAGE:
                // Handle case where the two subpages belong to
                // different sections, but are linked to each other. For example
                // /displayAndMagnification linking to /display
                if (!oldRoute.contains(newRoute) &&
                    !newRoute.contains(oldRoute)) {
                  this.enterMainPage().then(() => {
                    this.enterSubpage(newRoute);
                  });
                  return;
                }

                // Handle case of subpage to nested subpage navigation.
                if (oldRoute.contains(newRoute)) {
                  this.scroller_.scrollTop = 0;
                  return;
                }
                // When going from a nested subpage to its parent subpage,
                // the scroll position is automatically restored because we
                // focus the nested subpage's entry point.
                return;

              // Happens when the user navigates to a subpage via the search box
              // on the root page, and then clicks the back button.
              case RouteState.ROOT:
                this.enterMainPage().then(() => {
                  this.activateInitialPage();
                });
                return;

              // This is a supported case but there are currently no known
              // examples of this transition in Settings.
              case RouteState.DIALOG:
                this.enterMainPage();
                return;

              default:
                return;
            }
          }

          if (oldState === RouteState.DIALOG) {
            switch (newState) {
              // There are currently no known examples of this transition
              case RouteState.SUBPAGE:
                this.enterSubpage(newRoute);
                return;

              // There are currently no known examples of these transitions.
              // Update when a relevant use-case exists.
              case RouteState.ROOT:
              case RouteState.SECTION:
              case RouteState.DIALOG:
              default:
                return;
            }
          }
        }

        /**
         * Finds the settings page corresponding to the given route. If the
         * page is lazily loaded (ie. under the advanced section), then
         * force-render it.
         * Note: If the page resides within "advanced" settings, a
         * 'hide-container' event is fired (necessary to avoid flashing).
         * Callers are responsible for firing a 'show-container' event.
         */
        private ensurePageForRoute(route: Route):
            Promise<PageDisplayerElement> {
          const section = this.queryPage(route.section);
          if (section) {
            return Promise.resolve(section);
          }

          // The function to use to wait for <dom-if>s to render.
          const waitFn = beforeNextRender.bind(null, this);

          return new Promise(resolve => {
            if (isAdvancedRoute(route) || isAboutRoute(route)) {
              this.dispatchCustomEvent_('hide-container');
              waitFn(async () => {
                await this.loadAdvancedPage();
                resolve(castExists(this.queryPage(route.section)));
              });
            } else {
              waitFn(() => {
                resolve(castExists(this.queryPage(route.section)));
              });
            }
          });
        }

        /**
         * Queries for a page visibility element with the given |section| from
         * the shadow DOM.
         */
        private queryPage(section: Section|null): PageDisplayerElement|null {
          if (section === null) {
            return null;
          }
          return this.shadowRoot!.querySelector<PageDisplayerElement>(
              `page-displayer[section="${section}"]`);
        }

        private dispatchCustomEvent_(
            name: string, options?: CustomEventInit<unknown>): void {
          const event = new CustomEvent(
              name, {bubbles: true, composed: true, ...options});
          this.dispatchEvent(event);
        }
      }

      return MainPageMixinInternal;
    });
