// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview SearchableViewContainerMixin implements the search and
 * highlighting logic for a typical Settings plugin that has parent and child
 * views. It assumes that a cr-view-manager is used to switch between all
 * views, which are implemented as DOM siblings, regardress of whether they
 * are presented to the user in a parent/child relationship. Each child view is
 * expected to have a `[data-parent-view-id]` HTML attribute pointing to its
 * parent view. This is necessary to properly reveal parent views when search
 * results exist in child views, as well as to show search bubbles to guide the
 * user to the child view's entry point. Parent views are expected to inherit
 * from the SettingsViewMixin and properly overriding the
 * `getAssociatedControlFor()` method.
 *
 * Note: Current implementation assumes that there are no
 * parent/child/grandchild views, only parent/child.
 *
 * The exposed `shouldShowAll` computed property is meant to be bound to the
 * cr-view-manager's `show-all` attribute. `inSearchMode` is expected to be
 * populated by the parent element and is necessary to calculate
 * shouldShowAll's value.
 */

import {assert} from '//resources/js/assert.js';
import type {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {CrViewManagerElement} from 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import {dedupingMixin} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {Route, RouteObserverMixinInterface} from '../router.js';
import {RouteObserverMixin} from '../router.js';
import {combineSearchResults, getSearchManager, showBubble} from '../search_settings.js';
import type {SearchResult} from '../search_settings.js';
import type {SettingsPlugin} from '../settings_main/settings_plugin.js';
import type {SettingsViewMixinInterface} from '../settings_page/settings_view_mixin.js';


// Attribute added to a view when it should be hidden due to not having any
// search hits.
const HIDDEN_BY_SEARCH: string = 'hidden-by-search';

type Constructor<T> = new (...args: any[]) => T;

export const SearchableViewContainerMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<SearchableViewContainerMixinInterface> => {
      const superClassBase = RouteObserverMixin(superClass) as unknown as T;

      class SearchableViewContainerMixin extends superClassBase {
        static get properties() {
          return {
            inSearchMode: {
              type: Boolean,
              value: false,
            },

            currentRoute: {
              type: Object,
              value: null,
            },

            shouldShowAll: {
              type: Boolean,
              computed: 'computeShouldShowAll_(inSearchMode, currentRoute)',
            },
          };
        }

        declare inSearchMode: boolean;
        declare currentRoute: Route|null;
        declare shouldShowAll: boolean;

        private getCrViewManager_(): CrViewManagerElement {
          const viewManager = this.shadowRoot!.querySelector('cr-view-manager');
          assert(!!viewManager);
          return viewManager;
        }

        currentRouteChanged(route: Route) {
          this.currentRoute = route;
        }

        async searchContents(query: string): Promise<SearchResult> {
          // Firstly search all parent views to detect any search hits and
          // update their visibility.
          const parentViews = this.getCrViewManager_().querySelectorAll(
              '[slot=view]:not([data-parent-view-id])');
          const parentPromises = Array.from(parentViews).map(view => {
            return getSearchManager().search(query, view).then(request => {
              const result = request.getSearchResult();
              if (result.wasClearSearch) {
                view.removeAttribute(HIDDEN_BY_SEARCH);
                return result;
              }

              view.toggleAttribute(HIDDEN_BY_SEARCH, result.matchCount === 0);
              return result;
            });
          });

          // Wait for all parent promises to finish, to avoid any race
          // conditions when possibly revealing parent sections later.
          // For now assume that there are not nested child views.
          await Promise.all(parentPromises);

          // Secondly search all child views to detect any search hits and
          // update their parents visibility so that they can be reachable.
          const childViews =
              this.getCrViewManager_().querySelectorAll<HTMLElement>(
                  '[slot=view][data-parent-view-id]');
          const childPromises = Array.from(childViews).map(view => {
            return getSearchManager().search(query, view).then(request => {
              const result = request.getSearchResult();
              if (result.wasClearSearch || result.matchCount === 0) {
                return result;
              }

              // Find and reveal parent view even if it was hidden earlier.
              const parentView =
                  this.getCrViewManager_()
                      .querySelector<HTMLElement&SettingsViewMixinInterface>(
                          `#${view.dataset['parentViewId']}`);
              assert(parentView);
              parentView.removeAttribute(HIDDEN_BY_SEARCH);

              // Highlight the associated control for entering the child view to
              // guide the user.
              const associatedControl =
                  parentView.getAssociatedControlFor(view.id);
              showBubble(
                  associatedControl, result.matchCount, request.bubbles,
                  /*horizontallyCenter=*/ false);
              return result;
            });
          });

          return combineSearchResults(
              await Promise.all([...parentPromises, ...childPromises]));
        }

        private computeShouldShowAll_(): boolean {
          return this.inSearchMode && !!this.currentRoute &&
              !this.currentRoute.isSubpage();
        }
      }

      return SearchableViewContainerMixin;
    });

export interface SearchableViewContainerMixinInterface extends
    RouteObserverMixinInterface, SettingsPlugin {
  inSearchMode: boolean;
  shouldShowAll: boolean;
  currentRoute: Route|null;
}
