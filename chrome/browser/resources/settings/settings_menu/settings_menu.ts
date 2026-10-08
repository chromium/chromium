// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-menu' shows a menu with a hardcoded set of pages and subpages.
 */
import 'chrome://resources/cr_elements/cr_menu_selector/cr_menu_selector.js';
import 'chrome://resources/cr_elements/cr_ripple/cr_ripple.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import '../icons.html.js';
// <if expr="_google_chrome">
import '../internal/icons.html.js';

// </if>

import type {CrMenuSelectorElement} from 'chrome://resources/cr_elements/cr_menu_selector/cr_menu_selector.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {loadTimeData} from '../i18n_setup.js';
import type {MetricsBrowserProxy} from '../metrics_browser_proxy.js';
import {AutofillSettingsReferrer, MetricsBrowserProxyImpl} from '../metrics_browser_proxy.js';
import {pageVisibility} from '../page_visibility.js';
import type {PageVisibility} from '../page_visibility.js';
import type {Route} from '../router.js';
import {RouteObserverMixinLit, Router} from '../router.js';

import {getCss} from './settings_menu.css.js';
import {getHtml} from './settings_menu.html.js';

export interface SettingsMenuElement {
  $: {
    autofill: HTMLLinkElement,
    menu: CrMenuSelectorElement,
    people: HTMLLinkElement,
  };
}

const pathToActionMap: Map<string, string> = new Map([
  ['/people', 'SettingsMenu_PeopleClicked'],
  ['/autofill', 'SettingsMenu_AutofillClicked'],
  ['/privacy', 'SettingsMenu_PrivacyClicked'],
  ['/performance', 'SettingsMenu_PerformanceClicked'],
  ['/ai', 'SettingsMenu_AiPageEntryPointClicked'],
  ['/appearance', 'SettingsMenu_AppearanceClicked'],
  ['/search', 'SettingsMenu_SearchClicked'],
  ['/defaultBrowser', 'SettingsMenu_DefaultBrowserClicked'],
  ['/onStartup', 'SettingsMenu_OnStartupClicked'],
  ['/languages', 'SettingsMenu_LanguagesClicked'],
  ['/downloads', 'SettingsMenu_DownloadsClicked'],
  ['/accessibility', 'SettingsMenu_AccessibilityClicked'],
  ['/system', 'SettingsMenu_SystemClicked'],
  ['/reset', 'SettingsMenu_ResetClicked'],
  ['/help', 'SettingsMenu_AboutClicked'],
]);

const SettingsMenuElementBase = RouteObserverMixinLit(CrLitElement);

export class SettingsMenuElement extends SettingsMenuElementBase {
  static get is() {
    return 'settings-menu';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      collapsed: {
        type: Boolean,
        reflect: true,
      },

      /**
       * Dictionary defining page visibility.
       */
      pageVisibility_: {type: Object},

      showAiPage_: {type: Boolean},
    };
  }

  accessor collapsed: boolean = false;
  protected accessor pageVisibility_: PageVisibility|undefined = pageVisibility;
  private accessor showAiPage_: boolean = loadTimeData.getBoolean('showAiPage');

  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();

  protected shouldHideMenuItem_(visibility: boolean|object|undefined): boolean {
    return visibility === false;
  }

  protected showAiPageMenuItem_(): boolean {
    return this.showAiPage_ &&
        (!this.pageVisibility_ || this.pageVisibility_.ai !== false);
  }

  override currentRouteChanged(newRoute: Route) {
    // Focus the initially selected path.
    const anchors = this.shadowRoot.querySelectorAll('a');
    for (let i = 0; i < anchors.length; ++i) {
      // Purposefully grabbing the 'href' attribute and not the property.
      const pathname = anchors[i].getAttribute('href')!;
      const anchorRoute = Router.getInstance().getRouteForPath(pathname);
      if (anchorRoute && anchorRoute.contains(newRoute)) {
        this.setSelectedPath_(pathname);
        return;
      }
    }

    this.setSelectedPath_('');  // Nothing is selected.
  }

  focusFirstItem() {
    const firstFocusableItem = this.shadowRoot.querySelector<HTMLElement>(
        '[role=menuitem]:not([hidden])');
    if (firstFocusableItem) {
      firstFocusableItem.focus();
    }
  }

  /**
   * Prevent clicks on sidebar items from navigating. These are only links for
   * accessibility purposes, taps are handled separately.
   */
  protected onLinkClick_(event: Event) {
    if ((event.target as HTMLElement).matches('a:not(#extensionsLink)')) {
      event.preventDefault();
    }
  }

  /**
   * Keeps both menus in sync. `path` needs to come from
   * `element.getAttribute('href')`. Using `element.href` will not work as it
   * would pass the entire URL instead of just the path.
   */
  private setSelectedPath_(path: string) {
    this.$.menu.selected = path;
  }

  protected onIronActivate_(event: CustomEvent<{selected: string}>) {
    const path = event.detail.selected;
    this.setSelectedPath_(path);
    this.metricsBrowserProxy_.recordSettingsNavCategoryClicked();

    const action = pathToActionMap.get(path);
    if (action) {
      this.metricsBrowserProxy_.recordAction(action);
    }

    const route = Router.getInstance().getRouteForPath(path);
    assert(route, `settings-menu encountered invalid path '${path}'`);
    Router.getInstance().navigateTo(
        route, /* dynamicParams */ undefined, /* removeSearch */ true);
  }

  protected onExtensionsLinkClick_() {
    chrome.metricsPrivate.recordUserAction(
        'SettingsMenu_ExtensionsLinkClicked');
  }

  protected onAutofillClick_() {
    this.metricsBrowserProxy_.recordAutofillSettingsReferrer(
        'Autofill.YourSavedInfoSettingsPage.VisitReferrer',
        AutofillSettingsReferrer.SETTINGS_MENU);
  }

  protected hideBottomMenuSeparator_(): boolean {
    if (!this.pageVisibility_) {
      return false;
    }

    const visibilities = [
      this.pageVisibility_.languages,
      this.pageVisibility_.downloads,
      this.pageVisibility_.a11y,
      // <if expr="not is_chromeos">
      this.pageVisibility_.system,
      // </if>
      this.pageVisibility_.reset,
    ];
    return visibilities.every(visibility => visibility === false);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-menu': SettingsMenuElement;
  }
}

customElements.define(SettingsMenuElement.is, SettingsMenuElement);
