// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './accelerator_subsection.js';
import '../css/shortcut_customization_shared.css.js';

import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {afterNextRender, microTask, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AcceleratorLookupManager} from './accelerator_lookup_manager.js';
import {AcceleratorRowElement} from './accelerator_row.js';
import {AcceleratorSubsectionElement} from './accelerator_subsection.js';
import {getShortcutProvider} from './mojo_interface_provider.js';
import {RouteObserver, Router} from './router.js';
import {AcceleratorCategory, AcceleratorSubcategory} from './shortcut_types.js';
import {getTemplate} from './shortcuts_page.html.js';

/**
 * @fileoverview
 * 'shortcuts-page' is a generic page that is capable of rendering the
 * shortcuts for a specific category.
 */

// 150ms is enough of delay to wait for the virtual keyboard to disappear and
// resume with a smooth scroll.
const kDefaultScrollTimeout = 150;

export class ShortcutsPageElement extends PolymerElement implements
    RouteObserver {
  static get is(): string {
    return 'shortcuts-page';
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       * Implicit property from NavigationSelector. Contains one Number field,
       * |category|, that holds the category type of this shortcut page.
       */
      initialData: {
        type: Object,
      },

      subcategories: {
        type: Array,
        value: [],
      },
    };
  }

  initialData: {category: AcceleratorCategory}|null;
  subcategories: AcceleratorSubcategory[];
  private scrollTimeout: number = kDefaultScrollTimeout;
  private lookupManager: AcceleratorLookupManager =
      AcceleratorLookupManager.getInstance();

  override connectedCallback(): void {
    super.connectedCallback();
    this.updateAccelerators();
    Router.getInstance().addObserver(this);
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();
    Router.getInstance().removeObserver(this);
  }

  updateAccelerators(): void {
    if (!this.initialData) {
      return;
    }

    const subcatMap =
        this.lookupManager.getSubcategories(this.initialData.category);
    if (subcatMap === undefined) {
      return;
    }

    const subcategories: number[] = [];
    for (const key of subcatMap.keys()) {
      subcategories.push(key);
    }
    this.subcategories = subcategories;
  }

  private getAllSubsections(): NodeListOf<AcceleratorSubsectionElement> {
    const subsections =
        this.shadowRoot!.querySelectorAll('accelerator-subsection');
    assert(subsections);
    return subsections;
  }

  updateSubsections(): void {
    for (const subsection of this.getAllSubsections()) {
      subsection.updateSubsection();
    }
  }

  /**
   * 'navigation-view-panel' is responsible for calling this function when
   * the active page changes.
   * @param isActive True if this page is the new active page.
   */
  onNavigationPageChanged({isActive}: {isActive: boolean}): void {
    if (isActive) {
      afterNextRender(this, () => {
        if (this.initialData) {
          getShortcutProvider().recordMainCategoryNavigation(
              this.initialData.category);
        }
        // Dispatch a custom event to inform the parent to scroll to the top
        // after active page changes.
        this.dispatchEvent(new CustomEvent('scroll-to-top', {
          bubbles: true,
          composed: true,
        }));

        // Scroll to the specific accelerator if this page change was caused by
        // clicking on a search result. If the page change was manual, the
        // method below will be a no-op.
        const didScroll = this.maybeScrollToAcceleratorRowBasedOnUrl(
            new URL(window.location.href));

        if (didScroll) {
          // Reset the route after scrolling so the app doesn't re-scroll when
          // the user manually changes pages.
          Router.getInstance().resetRoute();
        }
      });
    }
  }

  /**
   * This method is called by the Router when the URL is updated via
   * the Router's `navigateTo` method.
   *
   * For this element, listening to route changes allows it to potentially
   * scroll to one of its child accelerators if the URL contains the correct
   * search params. This will happen if the route change was caused by selecting
   * a search result.
   */
  onRouteChanged(url: URL): void {
    const didScroll = this.maybeScrollToAcceleratorRowBasedOnUrl(url);
    if (didScroll) {
      // Reset the route after scrolling so the app doesn't re-scroll when
      // the user manually changes pages.
      Router.getInstance().resetRoute();
    }
  }

  /**
   * Scroll the URL-selected accelerator to the top of the page.
   * If the URL does not contain the correct search params (`action` and
   * `category`), then this method is a no-op.
   * @returns True if the scroll event happened.
   */
  private maybeScrollToAcceleratorRowBasedOnUrl(url: URL): boolean {
    const action = url.searchParams.get('action');
    const category = url.searchParams.get('category');
    if (!action || !category) {
      // This route change did not include the params that would trigger a
      // scroll event.
      return false;
    }

    if (this.initialData?.category.toString() !== category) {
      // Only focus the element if we're in the correct category.
      return false;
    }

    const acceleratorSubsections =
        this.shadowRoot?.querySelectorAll<AcceleratorSubsectionElement>(
            'accelerator-subsection');
    assert(
        acceleratorSubsections,
        'Expected this element to contain accelerator-subsection elements.');

    for (let i = 0; i < acceleratorSubsections.length; i++) {
      const matchingAcceleratorRow =
          acceleratorSubsections[i]
              .shadowRoot?.querySelector<AcceleratorRowElement>(
                  `accelerator-row[action="${action}"]`);
      if (matchingAcceleratorRow) {
        // Use microtask timing to ensure that the scrolling action happens.
        microTask.run(() => {
          // Note: There is a bug in which the onscreen virtual keyboard's close
          // animation conflicts with smooth scrolling. Adding a 150ms handles
          // the issue by waiting for the virtual keyboard close animation
          // to finish.
          if (this.scrollTimeout === 0) {
            // Don't queue a timeout if there is no delay to be added. A zero
            // timeout will still make the function an asynchronous call.
            // This removes the need to use a flaky timeout for tests.
            matchingAcceleratorRow.scrollIntoView({behavior: 'smooth'});
          } else {
            setTimeout(() => {
              matchingAcceleratorRow.scrollIntoView({behavior: 'smooth'});
            }, this.scrollTimeout);
          }
        });

        // Focus on the matching accelerator row.
        strictQuery(
            '#container', matchingAcceleratorRow.shadowRoot,
            HTMLTableRowElement)
            .focus();
        this.lookupManager.setSearchResultRowFocused(true);

        // The scroll event did happen, so return true.
        return true;
      }
    }

    return false;
  }

  setScrollTimeoutForTesting(timeout: number): void {
    this.scrollTimeout = timeout;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }
}

customElements.define(ShortcutsPageElement.is, ShortcutsPageElement);
