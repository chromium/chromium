// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview SettingsViewMixinLit is meant to be inherited by parent or
 * child views belonging to a Settings plugin, to help with
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
import type {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import type {FocusConfig} from '../focus_config.js';
import type {Route, RouteObserverMixinInterface} from '../router.js';
import {Router} from '../router.js';

type Constructor<T> = new (...args: any[]) => T;

export const SettingsViewMixinLit = <T extends Constructor<CrLitElement>>(
    superClass: T): T&Constructor<SettingsViewMixinLitInterface> => {
  class SettingsViewMixinLit extends superClass implements
      RouteObserverMixinInterface {
    static get properties() {
      return {
        routePath: {type: String},
      };
    }

    accessor routePath: string = '';

    private focusConfig_: FocusConfig|null = null;
    private previousRoute_: Route|null = null;
    private onViewEnterStartListener_: EventListener = () =>
        this.onViewEnterStart_();

    override connectedCallback() {
      super.connectedCallback();

      Router.getInstance().addObserver(this);
      this.currentRouteChanged(
          Router.getInstance().getCurrentRoute(), undefined);

      this.focusConfig_ = this.getFocusConfig();
      this.addEventListener('view-enter-start', this.onViewEnterStartListener_);
    }

    override disconnectedCallback() {
      super.disconnectedCallback();

      Router.getInstance().removeObserver(this);
      this.removeEventListener(
          'view-enter-start', this.onViewEnterStartListener_);
    }

    // Should be overridden by views that have a back button (usually
    // subpage views).
    focusBackButton() {}

    // Should be overridden by views that have subpage views, to control
    // what gets focused when coming back from a subpage.
    getFocusConfig(): FocusConfig|null {
      return null;
    }

    // Should be overridden by views that have subpage views, to specify
    // which element should be highlighted when a search hit occurs in a
    // subpage view.
    getAssociatedControlFor(_childViewId: string): HTMLElement {
      assertNotReached();
    }

    private onViewEnterStart_() {
      if (this.previousRoute_ &&
          !Router.getInstance().lastRouteChangeWasPopstate()) {
        this.focusBackButton();
        return;
      }

      if (!Router.getInstance().lastRouteChangeWasPopstate()) {
        return;
      }

      if (!this.focusConfig_ || !this.previousRoute_) {
        return;
      }

      const currentRoute = Router.getInstance().getCurrentRoute();
      const fromToKey = `${this.previousRoute_.path}_${currentRoute.path}`;

      let pathConfig = this.focusConfig_.get(fromToKey) ||
          this.focusConfig_.get(this.previousRoute_.path);
      if (pathConfig) {
        let handler;
        if (typeof pathConfig === 'function') {
          handler = pathConfig;
        } else {
          handler = () => {
            if (typeof pathConfig === 'string') {
              const element = this.shadowRoot.querySelector(pathConfig);
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

  return SettingsViewMixinLit;
};

export interface SettingsViewMixinLitInterface extends
    RouteObserverMixinInterface {
  routePath: string;
  focusBackButton(): void;
  getFocusConfig(): FocusConfig|null;
  getAssociatedControlFor(childViewId: string): HTMLElement;
}
