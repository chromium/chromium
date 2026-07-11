// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview SearchableViewContainerMixinLit implements the search and
 * highlighting logic for a typical Settings plugin that has parent and child
 * views.
 */

import {assert} from '//resources/js/assert.js';
import type {CrLitElement, PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {CrViewManagerElement} from 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';

import type {Route, RouteObserverMixinInterface} from '../router.js';
import {RouteObserverMixinLit} from '../router.js';
import {combineSearchResults, getSearchManager, showBubble} from '../search_settings.js';
import type {SearchResult} from '../search_settings.js';
import type {SettingsPlugin} from '../settings_main/settings_plugin.js';

import type {SettingsViewMixinInterface} from './settings_view_mixin.js';

// Attribute added to a view when it should be hidden due to not having any
// search hits.
const HIDDEN_BY_SEARCH: string = 'hidden-by-search';

type Constructor<T> = new (...args: any[]) => T;

export const SearchableViewContainerMixinLit =
    <T extends Constructor<CrLitElement>>(superClass: T): T&
    Constructor<SearchableViewContainerMixinLitInterface> => {
      const superClassBase = RouteObserverMixinLit(superClass) as unknown as T;

      class SearchableViewContainerMixinLit extends superClassBase implements
          SearchableViewContainerMixinLitInterface {
        static get properties() {
          return {
            inSearchMode: {type: Boolean},
            currentRoute: {type: Object},
            shouldShowAll: {type: Boolean},
          };
        }

        accessor inSearchMode: boolean = false;
        accessor currentRoute: Route|null = null;
        accessor shouldShowAll: boolean = false;

        override willUpdate(changedProperties: PropertyValues<this>) {
          super.willUpdate(changedProperties);

          if (changedProperties.has('inSearchMode') ||
              changedProperties.has('currentRoute')) {
            this.shouldShowAll = this.inSearchMode && !!this.currentRoute &&
                !this.currentRoute.isSubpage();
          }
        }

        private getCrViewManager_(): CrViewManagerElement {
          const viewManager = this.shadowRoot.querySelector('cr-view-manager');
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
          const parentsResult =
              combineSearchResults(await Promise.all(parentPromises));
          if (parentsResult.canceled) {
            return parentsResult;
          }

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
      }

      return SearchableViewContainerMixinLit;
    };

export interface SearchableViewContainerMixinLitInterface extends
    RouteObserverMixinInterface, SettingsPlugin {
  inSearchMode: boolean;
  shouldShowAll: boolean;
  currentRoute: Route|null;
}
