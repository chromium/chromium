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
import '../../settings_shared.css.js';
import '../os_settings_icons.html.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {IronCollapseElement} from 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import {IronSelectorElement} from 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from '../assert_extras.js';
import {routes} from '../os_settings_routes.js';
import {RouteObserverMixin} from '../route_observer_mixin.js';
import {Route, Router} from '../router.js';

import {getTemplate} from './os_settings_menu.html.js';

interface OsSettingsMenuElement {
  $: {
    topMenu: IronSelectorElement,
    subMenu: IronSelectorElement,
    advancedSubmenu: IronCollapseElement,
  };
}

const OsSettingsMenuElementBase = RouteObserverMixin(PolymerElement);

class OsSettingsMenuElement extends OsSettingsMenuElementBase {
  static get is() {
    return 'os-settings-menu';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      advancedOpened: {
        type: Boolean,
        value: false,
        notify: true,
      },

      /**
       * Whether the user is in guest mode.
       */
      isGuestMode_: {
        type: Boolean,
        value: loadTimeData.getBoolean('isGuest'),
        readOnly: true,
      },

      showCrostini: Boolean,

      showStartup: Boolean,

      showReset: Boolean,

      showKerberosSection: Boolean,
    };
  }

  advancedOpened: boolean;
  showCrostini: boolean;
  showStartup: boolean;
  showReset: boolean;
  showKerberosSection: boolean;
  private isGuestMode_: boolean;

  override currentRouteChanged(newRoute: Route) {
    const urlSearchQuery =
        Router.getInstance().getQueryParameters().get('search');
    // If the route navigated to by a search result is in the advanced
    // section, the advanced menu will expand.
    if (urlSearchQuery && routes.ADVANCED &&
        routes.ADVANCED.contains(newRoute)) {
      this.advancedOpened = true;
    }

    // Focus the initially selected path.
    const anchors = this.shadowRoot!.querySelectorAll('a');
    for (let i = 0; i < anchors.length; ++i) {
      const href = castExists(anchors[i].getAttribute('href'));
      const anchorRoute = Router.getInstance().getRouteForPath(href);
      if (anchorRoute && anchorRoute.contains(newRoute)) {
        this.setSelectedUrl_(anchors[i].href);
        return;
      }
    }

    this.setSelectedUrl_('');  // Nothing is selected.
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
   * Keeps both menus in sync. |url| needs to come from |element.href| because
   * |iron-list| uses the entire url. Using |getAttribute| will not work.
   */
  private setSelectedUrl_(url: string) {
    this.$.topMenu.selected = this.$.subMenu.selected = url;
  }

  private onSelectorActivate_(event: CustomEvent<{selected: string}>) {
    this.setSelectedUrl_(event.detail.selected);
  }

  /**
   * @param opened Whether the menu is expanded.
   * @return Which icon to use.
   */
  private arrowState_(opened: boolean): string {
    return opened ? 'cr:arrow-drop-up' : 'cr:arrow-drop-down';
  }

  /** @return Whether the advanced submenu is open. */
  isAdvancedSubmenuOpenedForTest(): boolean {
    return this.$.advancedSubmenu.opened;
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
