// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-menu' shows a menu with a hardcoded set of pages and subpages.
 */
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_menu_selector/cr_menu_selector.js';
import 'chrome://resources/cr_elements/cr_nav_menu_item_style.css.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/paper-ripple/paper-ripple.js';
import '../icons.html.js';
import '../settings_shared.css.js';

import {CrMenuSelector} from 'chrome://resources/cr_elements/cr_menu_selector/cr_menu_selector.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {DomIf, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PageVisibility} from '../page_visibility.js';
import {routes} from '../route.js';
import {Route, RouteObserverMixin, RouteObserverMixinInterface, Router} from '../router.js';

import {getTemplate} from './settings_menu.html.js';

export interface SettingsMenuElement {
  $: {
    autofill: HTMLLinkElement,
    menu: CrMenuSelector,
    people: HTMLLinkElement,
  };
}

const SettingsMenuElementBase = RouteObserverMixin(PolymerElement) as
    {new (): PolymerElement & RouteObserverMixinInterface};

export class SettingsMenuElement extends SettingsMenuElementBase {
  static get is() {
    return 'settings-menu';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Dictionary defining page visibility.
       */
      pageVisibility: Object,

      performanceFeaturesAvailable_: {
        type: Boolean,
        value: function() {
          return loadTimeData.getBoolean('highEfficiencyModeAvailable') ||
              loadTimeData.getBoolean('batterySaverModeAvailable');
        },
      },
    };
  }

  pageVisibility: PageVisibility;
  private performanceFeaturesAvailable_: boolean;

  override currentRouteChanged(newRoute: Route) {
    if (this.performanceFeaturesAvailable_ && newRoute === routes.PERFORMANCE) {
      // Add special handling for the Performance section, since the
      // corresponding menu entry resides in a dom-if and is normally not
      // present in the DOM during initial load. Force-render the dom-if
      // instead.
      const anchor = this.shadowRoot!.querySelector('#performance');
      if (anchor === null) {
        const domIf =
            this.shadowRoot!.querySelector<DomIf>('#performanceDomIf');
        assert(domIf);
        assert(domIf.if);
        domIf.render();
      }
    }

    // Focus the initially selected path.
    const anchors = this.shadowRoot!.querySelectorAll('a');
    for (let i = 0; i < anchors.length; ++i) {
      const anchorRoute = Router.getInstance().getRouteForPath(
          anchors[i].getAttribute('href')!);
      if (anchorRoute && anchorRoute.contains(newRoute)) {
        this.setSelectedUrl_(anchors[i].href);
        return;
      }
    }

    this.setSelectedUrl_('');  // Nothing is selected.
  }

  focusFirstItem() {
    const firstFocusableItem = this.shadowRoot!.querySelector<HTMLElement>(
        '[role=menuitem]:not([hidden])');
    if (firstFocusableItem) {
      firstFocusableItem.focus();
    }
  }

  /**
   * Prevent clicks on sidebar items from navigating. These are only links for
   * accessibility purposes, taps are handled separately by <iron-selector>.
   */
  private onLinkClick_(event: Event) {
    if ((event.target as HTMLElement).matches('a:not(#extensionsLink)')) {
      event.preventDefault();
    }
  }

  /**
   * Keeps both menus in sync. |url| needs to come from |element.href| because
   * |iron-list| uses the entire url. Using |getAttribute| will not work.
   */
  private setSelectedUrl_(url: string) {
    this.$.menu.selected = url;
  }

  private onSelectorActivate_(event: CustomEvent<{selected: string}>) {
    this.setSelectedUrl_(event.detail.selected);

    const path = new URL(event.detail.selected).pathname;
    const route = Router.getInstance().getRouteForPath(path);
    assert(route, 'settings-menu has an entry with an invalid route.');
    Router.getInstance().navigateTo(
        route!, /* dynamicParams */ undefined, /* removeSearch */ true);
  }

  private onExtensionsLinkClick_() {
    chrome.metricsPrivate.recordUserAction(
        'SettingsMenu_ExtensionsLinkClicked');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-menu': SettingsMenuElement;
  }
}

customElements.define(SettingsMenuElement.is, SettingsMenuElement);
