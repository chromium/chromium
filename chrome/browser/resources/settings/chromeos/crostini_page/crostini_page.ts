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

import {I18nMixin, I18nMixinInterface} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin, WebUiListenerMixinInterface} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Setting} from '../../mojom-webui/setting.mojom-webui.js';
import {PrefsMixin, PrefsMixinInterface} from '../../prefs/prefs_mixin.js';
import {DeepLinkingBehavior, DeepLinkingBehaviorInterface} from '../deep_linking_behavior.js';
import {routes} from '../os_route.js';
import {RouteObserverMixin, RouteObserverMixinInterface} from '../route_observer_mixin.js';
import {Route, Router} from '../router.js';

import {CrostiniBrowserProxy, CrostiniBrowserProxyImpl} from './crostini_browser_proxy.js';
import {getTemplate} from './crostini_page.html.js';

const SettingsCrostiniPageElementBase =
    mixinBehaviors(
        [
          DeepLinkingBehavior,
        ],
        PrefsMixin(RouteObserverMixin(
            I18nMixin(WebUiListenerMixin(PolymerElement))))) as {
      new (): PolymerElement & WebUiListenerMixinInterface &
          I18nMixinInterface & RouteObserverMixinInterface &
          PrefsMixinInterface & DeepLinkingBehaviorInterface,
    };

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
       * Used by DeepLinkingBehavior to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set([Setting.kSetUpCrostini]),
      },

      enableBruschetta_: {
        type: Boolean,
        value: loadTimeData.getBoolean('enableBruschetta'),
      },
    };
  }

  private browserProxy_: CrostiniBrowserProxy;
  private disableCrostiniInstall_: boolean;
  private enableBruschetta_: boolean;

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

  private onBruschettaSubpageTap_() {
    if (this.enableBruschetta_) {
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
