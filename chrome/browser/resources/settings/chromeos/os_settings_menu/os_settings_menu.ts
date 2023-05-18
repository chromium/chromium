// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-menu' shows a menu with a hardcoded set of pages and subpages.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';
import '../settings_shared.css.js';
import '../os_settings_icons.html.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {IronCollapseElement} from 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import {IronSelectorElement} from 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';
import {DomRepeat, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import * as routesMojom from '../mojom-webui/routes.mojom-webui.js';
import {OsPageAvailability} from '../os_page_availability.js';
import {routes} from '../os_settings_routes.js';
import {RouteObserverMixin} from '../route_observer_mixin.js';
import {Route, Router} from '../router.js';

import {getTemplate} from './os_settings_menu.html.js';

interface MenuItemData {
  pageName: keyof OsPageAvailability;
  path: string;
  icon: string;
  label: string;
}

export interface OsSettingsMenuElement {
  $: {
    topMenu: IronSelectorElement,
    topMenuRepeat: DomRepeat,
    subMenu: IronSelectorElement,
    advancedSubmenu: IronCollapseElement,
  };
}

const OsSettingsMenuElementBase = RouteObserverMixin(I18nMixin(PolymerElement));

export class OsSettingsMenuElement extends OsSettingsMenuElementBase {
  static get is() {
    return 'os-settings-menu';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Determines which menu items are available for their respective pages
       */
      pageAvailability: {
        type: Object,
      },

      advancedOpened: {
        type: Boolean,
        value: false,
        notify: true,
      },

      basicMenuItems_: {
        type: Array,
        computed: 'computeBasicMenuItems_(pageAvailability.*)',
        readOnly: true,
      },

      advancedMenuItems_: {
        type: Array,
        computed: 'computeAdvancedMenuItems_(pageAvailability.*)',
        readOnly: true,
      },

      /**
       * The full URL (e.g. chrome://os-settings/internet) of the currently
       * selected menu item. Not to be confused with the href attribute.
       */
      selectedUrl_: {
        type: String,
        value: '',
      },
    };
  }

  advancedOpened: boolean;
  pageAvailability: OsPageAvailability;
  private basicMenuItems_: MenuItemData[];
  private advancedMenuItems_: MenuItemData[];
  private selectedUrl_: string;

  override ready(): void {
    super.ready();

    // Force render menu items so the matching item can be selected when the
    // page initially loads
    this.$.topMenuRepeat.render();
  }

  override currentRouteChanged(newRoute: Route) {
    const urlSearchQuery =
        Router.getInstance().getQueryParameters().get('search');
    // If the route navigated to by a search result is in the advanced
    // section, the advanced menu will expand.
    if (urlSearchQuery && routes.ADVANCED &&
        routes.ADVANCED.contains(newRoute)) {
      this.advancedOpened = true;
    }

    this.setSelectedUrlFromRoute_(newRoute);
  }

  /**
   * Set the selected menu item based on the current route path matching the
   * href attribute.
   */
  private setSelectedUrlFromRoute_(route: Route) {
    const anchors =
        this.shadowRoot!.querySelectorAll<HTMLAnchorElement>('a.item');
    for (const anchor of anchors) {
      const path = new URL(anchor.href).pathname;
      const matchingRoute = Router.getInstance().getRouteForPath(path);
      if (matchingRoute?.contains(route)) {
        this.setSelectedUrl_(anchor.href);
        return;
      }
    }

    this.setSelectedUrl_('');  // Nothing is selected.
  }

  private computeBasicMenuItems_(): MenuItemData[] {
    const basicMenuItems: MenuItemData[] = [
      {
        pageName: 'internet',
        path: routesMojom.NETWORK_SECTION_PATH,
        icon: 'os-settings:network-wifi',
        label: this.i18n('internetPageTitle'),
      },
      {
        pageName: 'bluetooth',
        path: routesMojom.BLUETOOTH_SECTION_PATH,
        icon: 'cr:bluetooth',
        label: this.i18n('bluetoothPageTitle'),
      },
      {
        pageName: 'multidevice',
        path: routesMojom.MULTI_DEVICE_SECTION_PATH,
        icon: 'os-settings:multidevice-better-together-suite',
        label: this.i18n('multidevicePageTitle'),
      },
      {
        pageName: 'osPeople',
        path: routesMojom.PEOPLE_SECTION_PATH,
        icon: 'cr:person',
        label: this.i18n('osPeoplePageTitle'),
      },
      {
        pageName: 'kerberos',
        path: routesMojom.KERBEROS_SECTION_PATH,
        icon: 'os-settings:auth-key',
        label: this.i18n('kerberosPageTitle'),
      },
      {
        pageName: 'device',
        path: routesMojom.DEVICE_SECTION_PATH,
        icon: 'os-settings:laptop-chromebook',
        label: this.i18n('devicePageTitle'),
      },
      {
        pageName: 'personalization',
        path: routesMojom.PERSONALIZATION_SECTION_PATH,
        icon: 'os-settings:paint-brush',
        label: this.i18n('personalizationPageTitle'),
      },
      {
        pageName: 'osSearch',
        path: routesMojom.SEARCH_AND_ASSISTANT_SECTION_PATH,
        icon: 'cr:search',
        label: this.i18n('osSearchPageTitle'),
      },
      {
        pageName: 'osPrivacy',
        path: routesMojom.PRIVACY_AND_SECURITY_SECTION_PATH,
        icon: 'cr:security',
        label: this.i18n('privacyPageTitle'),
      },
      {
        pageName: 'apps',
        path: routesMojom.APPS_SECTION_PATH,
        icon: 'os-settings:apps',
        label: this.i18n('appsPageTitle'),
      },
      {
        pageName: 'osAccessibility',
        path: routesMojom.ACCESSIBILITY_SECTION_PATH,
        icon: 'os-settings:accessibility',
        label: this.i18n('a11yPageTitle'),
      },
    ];

    return basicMenuItems.filter(
        ({pageName}) => !!this.pageAvailability[pageName]);
  }

  private computeAdvancedMenuItems_(): MenuItemData[] {
    const advancedMenuItems: MenuItemData[] = [
      {
        pageName: 'dateTime',
        path: routesMojom.DATE_AND_TIME_SECTION_PATH,
        icon: 'os-settings:access-time',
        label: this.i18n('dateTimePageTitle'),
      },
      {
        pageName: 'osLanguages',
        path: routesMojom.LANGUAGES_AND_INPUT_SECTION_PATH,
        icon: 'os-settings:language',
        label: this.i18n('osLanguagesPageTitle'),
      },
      {
        pageName: 'files',
        path: routesMojom.FILES_SECTION_PATH,
        icon: 'os-settings:folder-outline',
        label: this.i18n('filesPageTitle'),
      },
      {
        pageName: 'osPrinting',
        path: routesMojom.PRINTING_SECTION_PATH,
        icon: 'os-settings:print',
        label: this.i18n('printingPageTitle'),
      },
      {
        pageName: 'crostini',
        path: routesMojom.CROSTINI_SECTION_PATH,
        icon: 'os-settings:developer-tags',
        label: this.i18n('crostiniPageTitle'),
      },
      {
        pageName: 'osReset',
        path: routesMojom.RESET_SECTION_PATH,
        icon: 'os-settings:restore',
        label: this.i18n('resetPageTitle'),
      },
    ];

    return advancedMenuItems.filter(
        ({pageName}) => !!this.pageAvailability[pageName]);
  }

  private onAdvancedButtonToggle_() {
    this.advancedOpened = !this.advancedOpened;
  }

  /**
   * Prevent clicks on sidebar items from navigating. These are only links for
   * accessibility purposes, taps are handled separately by <iron-selector>.
   */
  private onLinkClick_(event: Event) {
    if ((event.target as HTMLElement).matches('a')) {
      event.preventDefault();
    }
  }

  /**
   * |iron-selector| expects a full URL so |element.href| is needed instead of
   * |element.getAttribute('href')|.
   */
  private setSelectedUrl_(url: string): void {
    this.selectedUrl_ = url;
  }

  private onItemActivated_(event: CustomEvent<{selected: string}>): void {
    this.setSelectedUrl_(event.detail.selected);
  }

  private onItemSelected_(e: CustomEvent<{item: HTMLElement}>) {
    e.detail.item.setAttribute('aria-current', 'true');
  }

  private onItemDeselected_(e: CustomEvent<{item: HTMLElement}>) {
    e.detail.item.removeAttribute('aria-current');
  }

  /**
   * @param opened Whether the menu is expanded.
   * @return Which icon to use.
   */
  private arrowState_(opened: boolean): string {
    return opened ? 'cr:arrow-drop-up' : 'cr:arrow-drop-down';
  }

  private boolToString_(bool: boolean): string {
    return bool.toString();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'os-settings-menu': OsSettingsMenuElement;
  }
}

customElements.define(OsSettingsMenuElement.is, OsSettingsMenuElement);
