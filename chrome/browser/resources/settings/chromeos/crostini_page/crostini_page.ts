// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'crostini-page' is the settings page for enabling Crostini.
 * Crostini Containers run Linux inside a Termina VM, allowing
 * the user to run Linux apps on their Chromebook.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/policy/cr_policy_indicator.js';
import '../os_settings_page/os_settings_animated_pages.js';
import '../os_settings_page/os_settings_subpage.js';
import '../../settings_shared.css.js';
import '../guest_os/guest_os_shared_paths.js';
import '../guest_os/guest_os_shared_usb_devices.js';
import 'chrome://resources/cr_components/localized_link/localized_link.js';
import './crostini_arc_adb.js';
import './crostini_export_import.js';
import './crostini_extra_containers.js';
import './crostini_port_forwarding.js';
import './crostini_shared_usb_devices.js';
import './crostini_subpage.js';
import './bruschetta_subpage.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Setting} from '../../mojom-webui/setting.mojom-webui.js';
import {PrefsMixin} from '../../prefs/prefs_mixin.js';
import {DeepLinkingMixin} from '../deep_linking_mixin.js';
import {routes} from '../os_settings_routes.js';
import {RouteObserverMixin} from '../route_observer_mixin.js';
import {Route, Router} from '../router.js';

import {CrostiniBrowserProxy, CrostiniBrowserProxyImpl} from './crostini_browser_proxy.js';
import {getTemplate} from './crostini_page.html.js';

const SettingsCrostiniPageElementBase = DeepLinkingMixin(PrefsMixin(
    RouteObserverMixin(I18nMixin(WebUiListenerMixin(PolymerElement)))));

class SettingsCrostiniPageElement extends SettingsCrostiniPageElementBase {
  static get is() {
    return 'settings-crostini-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      focusConfig_: {
        type: Object,
        value() {
          const map = new Map();
          if (routes.CROSTINI_DETAILS) {
            map.set(routes.CROSTINI_DETAILS.path, '#crostini .subpage-arrow');
          }
          if (routes.CROSTINI_DISK_RESIZE) {
            map.set(
                routes.CROSTINI_DISK_RESIZE.path, '#crostini .subpage-arrow');
          }
          if (routes.CROSTINI_EXPORT_IMPORT) {
            map.set(
                routes.CROSTINI_EXPORT_IMPORT.path, '#crostini .subpage-arrow');
          }
          if (routes.CROSTINI_EXTRA_CONTAINERS) {
            map.set(
                routes.CROSTINI_EXTRA_CONTAINERS.path,
                '#crostini .subpage-arrow');
          }
          if (routes.CROSTINI_PORT_FORWARDING) {
            map.set(
                routes.CROSTINI_PORT_FORWARDING.path,
                '#crostini .subpage-arrow');
          }
          if (routes.CROSTINI_SHARED_PATHS) {
            map.set(
                routes.CROSTINI_SHARED_PATHS.path, '#crostini .subpage-arrow');
          }
          if (routes.CROSTINI_SHARED_USB_DEVICES) {
            map.set(
                routes.CROSTINI_SHARED_USB_DEVICES.path,
                '#crostini .subpage-arrow');
          }
          if (routes.BRUSCHETTA_DETAILS) {
            map.set(routes.BRUSCHETTA_DETAILS.path, '#crostini .subpage-arrow');
          }
          if (routes.BRUSCHETTA_SHARED_USB_DEVICES) {
            map.set(
                routes.BRUSCHETTA_SHARED_USB_DEVICES.path,
                '#crostini .subpage-arrow');
          }
          if (routes.BRUSCHETTA_SHARED_PATHS) {
            map.set(
                routes.BRUSCHETTA_SHARED_PATHS.path,
                '#crostini .subpage-arrow');
          }
          return map;
        },
      },

      /**
       * Whether the install option should be enabled.
       */
      disableCrostiniInstall_: {
        type: Boolean,
      },

      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([Setting.kSetUpCrostini]),
      },

      showBruschetta_: {
        type: Boolean,
        value: loadTimeData.getBoolean('showBruschetta'),
      },
    };
  }

  private browserProxy_: CrostiniBrowserProxy;
  private disableCrostiniInstall_: boolean;

  constructor() {
    super();

    this.browserProxy_ = CrostiniBrowserProxyImpl.getInstance();
  }

  override connectedCallback() {
    super.connectedCallback();

    if (!loadTimeData.getBoolean('allowCrostini')) {
      this.disableCrostiniInstall_ = true;
      return;
    }
    this.addWebUiListener(
        'crostini-installer-status-changed', (installerShowing: boolean) => {
          this.disableCrostiniInstall_ = installerShowing;
        });
    this.browserProxy_.requestCrostiniInstallerStatus();
  }

  override currentRouteChanged(route: Route) {
    // Does not apply to this page.
    if (route !== routes.CROSTINI) {
      return;
    }

    this.attemptDeepLink();
  }

  private onEnableTap_(event: Event) {
    this.browserProxy_.requestCrostiniInstallerView();
    event.stopPropagation();
  }

  private onSubpageTap_(event: Event) {
    // We do not open the subpage if the click was on a link.
    if (event.target && (event.target as HTMLElement).tagName === 'A') {
      event.stopPropagation();
      return;
    }

    if (this.getPref('crostini.enabled.value')) {
      Router.getInstance().navigateTo(routes.CROSTINI_DETAILS);
    }
  }

  private onBruschettaEnableTap_(event: Event) {
    this.browserProxy_.requestBruschettaInstallerView();
    // Stop propagation so that onBruschettaSubpageTap_ isn't called.
    event.stopPropagation();
  }

  private onBruschettaSubpageTap_() {
    // This function is called on-click even if actionable=false.
    if (this.getPref('bruschetta.installed.value')) {
      Router.getInstance().navigateTo(routes.BRUSCHETTA_DETAILS);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-crostini-page': SettingsCrostiniPageElement;
  }
}

customElements.define(
    SettingsCrostiniPageElement.is, SettingsCrostiniPageElement);
