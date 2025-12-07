// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview SettingsViewMixin is meant to be inherited by parent or child
 * views belonging to a Settings plugin, to help with
 *  a) focus preservation during navigation
 *  b) search bubble showing during search
 *
 * If a parent view, need to override the following methods:
 *   - getFocusConfig(): Used when navigating 'back' to focus the correct
 *     element (the entry point to a child view).
 *   - getAssociatedControlFor(): Used by SearchableViewContainerMixin to query
 *     which element should be highlighted with a search bubble.
 *
 * If a child view, need to override the following method:
 *   - focusBackButton(): Called when navigating into a child view to focus the
 *     back button.
 */

import {assert, assertNotReached} from '//resources/js/assert.js';
import {focusWithoutInk} from '//resources/js/focus_without_ink.js';
import type {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {dedupingMixin} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {FocusConfig} from '../focus_config.js';
import type {Route, RouteObserverMixinInterface} from '../router.js';
import {RouteObserverMixin, Router} from '../router.js';

type Constructor<T> = new (...args: any[]) => T;

export const SettingsViewMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<SettingsViewMixinInterface> => {
      const superClassBase = RouteObserverMixin(superClass) as unknown as T;

      class SettingsViewMixin extends superClassBase {
        private focusConfig_: FocusConfig|null = null;
        private previousRoute_: Route|null = null;

        static get properties() {
          return {
            routePath: String,
          };
        }

        declare routePath: string;

        override ready() {
          super.ready();

          /**
           * A Map specifying which element should be focused when exiting a
           * subpage. The key of the map holds a Route path, and the value
           * holds either a query selector that identifies the desired
           * element, an element or a function to be run.
           */
          this.focusConfig_ = this.getFocusConfig();

          this.addEventListener('view-enter-start', this.onViewEnterStart_);
        }

        // Should be overridden by views that have a back button (usually
        // subpage views).
        focusBackButton() {}

        // Should be overridden by views that have subpage views, to control
        // what gets focused when coming back from a subpage.
        getFocusConfig() {
          return null;
        }

        // Should be overridden by views that have subpage views, to specify
        // which element should be highlighted when a search hit occurs in a
        // subpage view.
        getAssociatedControlFor(_childViewId: string): HTMLElement {
          // Must be overridden by subclasses.
          assertNotReached();
        }

        private onViewEnterStart_() {
          // Call focusBackButton() on the selected subpage, only if:
          //  1) Not a direct navigation (such that the search box stays
          //     focused), and
          //  2) Not a "back" navigation, in which case the anchor element
          //     should be focused (further below in this function).
          if (this.previousRoute_ &&
              !Router.getInstance().lastRouteChangeWasPopstate()) {
            this.focusBackButton();
            return;
          }

          // Don't attempt to focus any anchor element, unless last navigation
          // was a 'pop' (backwards) navigation.
          if (!Router.getInstance().lastRouteChangeWasPopstate()) {
            return;
          }

          if (!this.focusConfig_ || !this.previousRoute_) {
            return;
          }

          const currentRoute = Router.getInstance().getCurrentRoute();
          const fromToKey = `${this.previousRoute_.path}_${currentRoute.path}`;

          // Look for a key that captures both previous and current route first.
          // If not found, then look for a key that only captures the previous
          // route.
          let pathConfig = this.focusConfig_.get(fromToKey) ||
              this.focusConfig_.get(this.previousRoute_.path);
          if (pathConfig) {
            let handler;
            if (typeof pathConfig === 'function') {
              handler = pathConfig;
            } else {
              handler = () => {
                if (typeof pathConfig === 'string') {
                  const element = this.shadowRoot!.querySelector(pathConfig);
                  assert(element);
                  pathConfig = element;
                }
                focusWithoutInk(pathConfig as HTMLElement);
              };
            }
            handler();
          }
        }

        currentRouteChanged(_newRoute: Route, oldRoute?: Route) {
          this.previousRoute_ = oldRoute || null;
        }
      }

      return SettingsViewMixin;
    });

export interface SettingsViewMixinInterface extends
    RouteObserverMixinInterface {
  focusBackButton(): void;
  getFocusConfig(): FocusConfig|null;
  getAssociatedControlFor(childViewId: string): HTMLElement;
}
