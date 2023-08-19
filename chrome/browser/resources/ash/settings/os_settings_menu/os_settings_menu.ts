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
import {IronSelectorElement} from 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';
import {DomRepeat, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {isRevampWayfindingEnabled} from '../common/load_time_booleans.js';
import * as routesMojom from '../mojom-webui/routes.mojom-webui.js';
import {OsPageAvailability} from '../os_page_availability.js';
import {RouteObserverMixin} from '../route_observer_mixin.js';
import {isAdvancedRoute, Route, Router} from '../router.js';

import {getTemplate} from './os_settings_menu.html.js';

const {Section} = routesMojom;

interface MenuItemData {
  section: routesMojom.Section;
  href: string;
  icon: string;
  label: string;
}

export interface OsSettingsMenuElement {
  $: {
    topMenu: IronSelectorElement,
    topMenuRepeat: DomRepeat,
  };
}

const OsSettingsMenuElementBase = RouteObserverMixin(I18nMixin(PolymerElement));

export class OsSettingsMenuElement extends OsSettingsMenuElementBase {
  static get is() {
    return 'os-settings-menu' as const;
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

      aboutMenuItemHref_: {
        type: String,
        value: `/${routesMojom.ABOUT_CHROME_OS_SECTION_PATH}`,
      },

      isRevampWayfindingEnabled_: {
        type: Boolean,
        value: () => {
          return isRevampWayfindingEnabled();
        },
        readOnly: true,
      },
    };
  }

  advancedOpened: boolean;
  pageAvailability: OsPageAvailability;
  private basicMenuItems_: MenuItemData[];
  private advancedMenuItems_: MenuItemData[];
  private isRevampWayfindingEnabled_: boolean;
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
    if (urlSearchQuery && isAdvancedRoute(newRoute)) {
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
    let basicMenuItems: MenuItemData[];
    if (this.isRevampWayfindingEnabled_) {
      basicMenuItems = [
        {
          section: Section.kNetwork,
          href: `/${routesMojom.NETWORK_SECTION_PATH}`,
          icon: 'os-settings:network-wifi',
          label: this.i18n('internetPageTitle'),
        },
        {
          section: Section.kBluetooth,
          href: `/${routesMojom.BLUETOOTH_SECTION_PATH}`,
          icon: 'cr:bluetooth',
          label: this.i18n('bluetoothPageTitle'),
        },
        {
          section: Section.kMultiDevice,
          href: `/${routesMojom.MULTI_DEVICE_SECTION_PATH}`,
          icon: 'os-settings:connected-devices',
          label: this.i18n('multidevicePageTitle'),
        },
        {
          section: Section.kPeople,
          href: `/${routesMojom.PEOPLE_SECTION_PATH}`,
          icon: 'os-settings:account',
          label: this.i18n('osPeoplePageTitle'),
        },
        {
          section: Section.kKerberos,
          href: `/${routesMojom.KERBEROS_SECTION_PATH}`,
          icon: 'os-settings:auth-key',
          label: this.i18n('kerberosPageTitle'),
        },
        {
          section: Section.kDevice,
          href: `/${routesMojom.DEVICE_SECTION_PATH}`,
          icon: 'os-settings:laptop-chromebook',
          label: this.i18n('devicePageTitle'),
        },
        {
          section: Section.kPersonalization,
          href: `/${routesMojom.PERSONALIZATION_SECTION_PATH}`,
          icon: 'os-settings:personalization',
          label: this.i18n('personalizationPageTitle'),
        },
        {
          section: Section.kPrivacyAndSecurity,
          href: `/${routesMojom.PRIVACY_AND_SECURITY_SECTION_PATH}`,
          icon: 'cr:security',
          label: this.i18n('privacyPageTitle'),
        },
        {
          section: Section.kApps,
          href: `/${routesMojom.APPS_SECTION_PATH}`,
          icon: 'os-settings:apps',
          label: this.i18n('appsPageTitle'),
        },
        {
          section: Section.kAccessibility,
          href: `/${routesMojom.ACCESSIBILITY_SECTION_PATH}`,
          icon: 'os-settings:accessibility-revamp',
          label: this.i18n('a11yPageTitle'),
        },
        {
          section: Section.kSystemPreferences,
          href: `/${routesMojom.SYSTEM_PREFERENCES_SECTION_PATH}`,
          icon: 'os-settings:system-preferences',
          label: this.i18n('systemPreferencesTitle'),
        },
      ];
    } else {
      basicMenuItems = [
        {
          section: Section.kNetwork,
          href: `/${routesMojom.NETWORK_SECTION_PATH}`,
          icon: 'os-settings:network-wifi',
          label: this.i18n('internetPageTitle'),
        },
        {
          section: Section.kBluetooth,
          href: `/${routesMojom.BLUETOOTH_SECTION_PATH}`,
          icon: 'cr:bluetooth',
          label: this.i18n('bluetoothPageTitle'),
        },
        {
          section: Section.kMultiDevice,
          href: `/${routesMojom.MULTI_DEVICE_SECTION_PATH}`,
          icon: 'os-settings:multidevice-better-together-suite',
          label: this.i18n('multidevicePageTitle'),
        },
        {
          section: Section.kPeople,
          href: `/${routesMojom.PEOPLE_SECTION_PATH}`,
          icon: 'cr:person',
          label: this.i18n('osPeoplePageTitle'),
        },
        {
          section: Section.kKerberos,
          href: `/${routesMojom.KERBEROS_SECTION_PATH}`,
          icon: 'os-settings:auth-key',
          label: this.i18n('kerberosPageTitle'),
        },
        {
          section: Section.kDevice,
          href: `/${routesMojom.DEVICE_SECTION_PATH}`,
          icon: 'os-settings:laptop-chromebook',
          label: this.i18n('devicePageTitle'),
        },
        {
          section: Section.kPersonalization,
          href: `/${routesMojom.PERSONALIZATION_SECTION_PATH}`,
          icon: 'os-settings:paint-brush',
          label: this.i18n('personalizationPageTitle'),
        },
        {
          section: Section.kSearchAndAssistant,
          href: `/${routesMojom.SEARCH_AND_ASSISTANT_SECTION_PATH}`,
          icon: 'cr:search',
          label: this.i18n('osSearchPageTitle'),
        },
        {
          section: Section.kPrivacyAndSecurity,
          href: `/${routesMojom.PRIVACY_AND_SECURITY_SECTION_PATH}`,
          icon: 'cr:security',
          label: this.i18n('privacyPageTitle'),
        },
        {
          section: Section.kApps,
          href: `/${routesMojom.APPS_SECTION_PATH}`,
          icon: 'os-settings:apps',
          label: this.i18n('appsPageTitle'),
        },
        {
          section: Section.kAccessibility,
          href: `/${routesMojom.ACCESSIBILITY_SECTION_PATH}`,
          icon: 'os-settings:accessibility',
          label: this.i18n('a11yPageTitle'),
        },
      ];
    }

    return basicMenuItems.filter(
        ({section}) => !!this.pageAvailability[section]);
  }

  private computeAdvancedMenuItems_(): MenuItemData[] {
    // When OsSettingsRevampWayfinding is enabled, there is no Advanced menu.
    if (this.isRevampWayfindingEnabled_) {
      return [];
    }

    const advancedMenuItems: MenuItemData[] = [
      {
        section: Section.kDateAndTime,
        href: `/${routesMojom.DATE_AND_TIME_SECTION_PATH}`,
        icon: 'os-settings:access-time',
        label: this.i18n('dateTimePageTitle'),
      },
      {
        section: Section.kLanguagesAndInput,
        href: `/${routesMojom.LANGUAGES_AND_INPUT_SECTION_PATH}`,
        icon: 'os-settings:language',
        label: this.i18n('osLanguagesPageTitle'),
      },
      {
        section: Section.kFiles,
        href: `/${routesMojom.FILES_SECTION_PATH}`,
        icon: 'os-settings:folder-outline',
        label: this.i18n('filesPageTitle'),
      },
      {
        section: Section.kPrinting,
        href: `/${routesMojom.PRINTING_SECTION_PATH}`,
        icon: 'os-settings:print',
        label: this.i18n('printingPageTitle'),
      },
      {
        section: Section.kCrostini,
        href: `/${routesMojom.CROSTINI_SECTION_PATH}`,
        icon: 'os-settings:developer-tags',
        label: this.i18n('crostiniPageTitle'),
      },
      {
        section: Section.kReset,
        href: `/${routesMojom.RESET_SECTION_PATH}`,
        icon: 'os-settings:restore',
        label: this.i18n('resetPageTitle'),
      },
    ];

    return advancedMenuItems.filter(
        ({section}) => !!this.pageAvailability[section]);
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
    [OsSettingsMenuElement.is]: OsSettingsMenuElement;
  }
}

customElements.define(OsSettingsMenuElement.is, OsSettingsMenuElement);
