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
import './menu_item.js';

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
  path: string;
  icon: string;
  label: string;

  // Sublabels should only exist when OsSettingsRevampWayfinding is enabled.
  sublabel?: string;
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
       * The path of the currently selected menu item. e.g. '/internet'.
       */
      selectedItemPath_: {
        type: String,
        value: '',
      },

      aboutMenuItemPath_: {
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
  private selectedItemPath_: string;
  private aboutMenuItemPath_: string;

  override ready(): void {
    super.ready();

    // Force render menu items so the matching item can be selected when the
    // page initially loads
    this.$.topMenuRepeat.render();
  }

  override currentRouteChanged(newRoute: Route): void {
    const urlSearchQuery =
        Router.getInstance().getQueryParameters().get('search');
    // If the route navigated to by a search result is in the advanced
    // section, the advanced menu will expand.
    if (urlSearchQuery && isAdvancedRoute(newRoute)) {
      this.advancedOpened = true;
    }

    this.setSelectedItemPathForRoute_(newRoute);
  }

  /**
   * Set the selected menu item based on a menu item's route matching or
   * containing the given |route|.
   */
  private setSelectedItemPathForRoute_(route: Route): void {
    const menuItems =
        this.shadowRoot!.querySelectorAll('os-settings-menu-item');
    for (const menuItem of menuItems) {
      const matchingRoute = Router.getInstance().getRouteForPath(menuItem.path);
      if (matchingRoute?.contains(route)) {
        this.setSelectedItemPath_(menuItem.path);
        return;
      }
    }

    // Nothing is selected.
    this.setSelectedItemPath_('');
  }

  private computeBasicMenuItems_(): MenuItemData[] {
    let basicMenuItems: MenuItemData[];
    if (this.isRevampWayfindingEnabled_) {
      basicMenuItems = [
        {
          section: Section.kNetwork,
          path: `/${routesMojom.NETWORK_SECTION_PATH}`,
          icon: 'os-settings:network-wifi',
          label: this.i18n('internetPageTitle'),
        },
        {
          section: Section.kBluetooth,
          path: `/${routesMojom.BLUETOOTH_SECTION_PATH}`,
          icon: 'cr:bluetooth',
          label: this.i18n('bluetoothPageTitle'),
        },
        {
          section: Section.kMultiDevice,
          path: `/${routesMojom.MULTI_DEVICE_SECTION_PATH}`,
          icon: 'os-settings:connected-devices',
          label: this.i18n('multidevicePageTitle'),
        },
        {
          section: Section.kPeople,
          path: `/${routesMojom.PEOPLE_SECTION_PATH}`,
          icon: 'os-settings:account',
          label: this.i18n('osPeoplePageTitle'),
        },
        {
          section: Section.kKerberos,
          path: `/${routesMojom.KERBEROS_SECTION_PATH}`,
          icon: 'os-settings:auth-key',
          label: this.i18n('kerberosPageTitle'),
        },
        {
          section: Section.kDevice,
          path: `/${routesMojom.DEVICE_SECTION_PATH}`,
          icon: 'os-settings:laptop-chromebook',
          label: this.i18n('devicePageTitle'),
        },
        {
          section: Section.kPersonalization,
          path: `/${routesMojom.PERSONALIZATION_SECTION_PATH}`,
          icon: 'os-settings:personalization',
          label: this.i18n('personalizationPageTitle'),
          sublabel: this.i18n('personalizationMenuItemDescription'),
        },
        {
          section: Section.kPrivacyAndSecurity,
          path: `/${routesMojom.PRIVACY_AND_SECURITY_SECTION_PATH}`,
          icon: 'cr:security',
          label: this.i18n('privacyPageTitle'),
        },
        {
          section: Section.kApps,
          path: `/${routesMojom.APPS_SECTION_PATH}`,
          icon: 'os-settings:apps',
          label: this.i18n('appsPageTitle'),
        },
        {
          section: Section.kAccessibility,
          path: `/${routesMojom.ACCESSIBILITY_SECTION_PATH}`,
          icon: 'os-settings:accessibility-revamp',
          label: this.i18n('a11yPageTitle'),
        },
        {
          section: Section.kSystemPreferences,
          path: `/${routesMojom.SYSTEM_PREFERENCES_SECTION_PATH}`,
          icon: 'os-settings:system-preferences',
          label: this.i18n('systemPreferencesTitle'),
        },
        {
          section: Section.kAboutChromeOs,
          path: this.aboutMenuItemPath_,
          icon: 'os-settings:chrome',
          label: this.i18n('aboutOsPageTitle'),
        },
      ];
    } else {
      basicMenuItems = [
        {
          section: Section.kNetwork,
          path: `/${routesMojom.NETWORK_SECTION_PATH}`,
          icon: 'os-settings:network-wifi',
          label: this.i18n('internetPageTitle'),
        },
        {
          section: Section.kBluetooth,
          path: `/${routesMojom.BLUETOOTH_SECTION_PATH}`,
          icon: 'cr:bluetooth',
          label: this.i18n('bluetoothPageTitle'),
        },
        {
          section: Section.kMultiDevice,
          path: `/${routesMojom.MULTI_DEVICE_SECTION_PATH}`,
          icon: 'os-settings:multidevice-better-together-suite',
          label: this.i18n('multidevicePageTitle'),
        },
        {
          section: Section.kPeople,
          path: `/${routesMojom.PEOPLE_SECTION_PATH}`,
          icon: 'cr:person',
          label: this.i18n('osPeoplePageTitle'),
        },
        {
          section: Section.kKerberos,
          path: `/${routesMojom.KERBEROS_SECTION_PATH}`,
          icon: 'os-settings:auth-key',
          label: this.i18n('kerberosPageTitle'),
        },
        {
          section: Section.kDevice,
          path: `/${routesMojom.DEVICE_SECTION_PATH}`,
          icon: 'os-settings:laptop-chromebook',
          label: this.i18n('devicePageTitle'),
        },
        {
          section: Section.kPersonalization,
          path: `/${routesMojom.PERSONALIZATION_SECTION_PATH}`,
          icon: 'os-settings:paint-brush',
          label: this.i18n('personalizationPageTitle'),
        },
        {
          section: Section.kSearchAndAssistant,
          path: `/${routesMojom.SEARCH_AND_ASSISTANT_SECTION_PATH}`,
          icon: 'cr:search',
          label: this.i18n('osSearchPageTitle'),
        },
        {
          section: Section.kPrivacyAndSecurity,
          path: `/${routesMojom.PRIVACY_AND_SECURITY_SECTION_PATH}`,
          icon: 'cr:security',
          label: this.i18n('privacyPageTitle'),
        },
        {
          section: Section.kApps,
          path: `/${routesMojom.APPS_SECTION_PATH}`,
          icon: 'os-settings:apps',
          label: this.i18n('appsPageTitle'),
        },
        {
          section: Section.kAccessibility,
          path: `/${routesMojom.ACCESSIBILITY_SECTION_PATH}`,
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
        path: `/${routesMojom.DATE_AND_TIME_SECTION_PATH}`,
        icon: 'os-settings:access-time',
        label: this.i18n('dateTimePageTitle'),
      },
      {
        section: Section.kLanguagesAndInput,
        path: `/${routesMojom.LANGUAGES_AND_INPUT_SECTION_PATH}`,
        icon: 'os-settings:language',
        label: this.i18n('osLanguagesPageTitle'),
      },
      {
        section: Section.kFiles,
        path: `/${routesMojom.FILES_SECTION_PATH}`,
        icon: 'os-settings:folder-outline',
        label: this.i18n('filesPageTitle'),
      },
      {
        section: Section.kPrinting,
        path: `/${routesMojom.PRINTING_SECTION_PATH}`,
        icon: 'os-settings:print',
        label: this.i18n('printingPageTitle'),
      },
      {
        section: Section.kCrostini,
        path: `/${routesMojom.CROSTINI_SECTION_PATH}`,
        icon: 'os-settings:developer-tags',
        label: this.i18n('crostiniPageTitle'),
      },
      {
        section: Section.kReset,
        path: `/${routesMojom.RESET_SECTION_PATH}`,
        icon: 'os-settings:restore',
        label: this.i18n('resetPageTitle'),
      },
    ];

    return advancedMenuItems.filter(
        ({section}) => !!this.pageAvailability[section]);
  }

  private onAdvancedButtonToggle_(): void {
    this.advancedOpened = !this.advancedOpened;
  }

  /**
   * @param path The path of the menu item to be selected. This path should be
   * the pathname portion of a URL, not the full URL. e.g. `/internet`, not
   * `chrome://os-settings/internet`.
   */
  private setSelectedItemPath_(path: string): void {
    this.selectedItemPath_ = path;
  }

  /**
   * Called when a selectable item from <iron-selector> is clicked. This is
   * fired before the selected item is changed.
   */
  private onItemActivated_(event: CustomEvent<{selected: string}>): void {
    this.setSelectedItemPath_(event.detail.selected);
  }

  private onItemSelected_(e: CustomEvent<{item: HTMLElement}>): void {
    e.detail.item.setAttribute('aria-current', 'true');
  }

  private onItemDeselected_(e: CustomEvent<{item: HTMLElement}>): void {
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
