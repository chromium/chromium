// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'crostini-page' is the settings page for enabling Crostini.
 * Crostini Containers run Linux inside a Termina VM, allowing
 * the user to run Linux apps on their Chromebook.
 */

import '//resources/cr_elements/cr_button/cr_button.m.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import '//resources/cr_elements/policy/cr_policy_indicator.m.js';
import '../../settings_page/settings_animated_pages.js';
import '../../settings_page/settings_subpage.js';
import '../../settings_shared_css.js';
import '../guest_os/guest_os_shared_paths.js';
import '../guest_os/guest_os_shared_usb_devices.js';
import '//resources/cr_components/localized_link/localized_link.js';
import './crostini_arc_adb.js';
import './crostini_export_import.js';
import './crostini_extra_containers.js';
import './crostini_port_forwarding.js';
import './crostini_subpage.js';
import './bruschetta_subpage.js';

import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';
import {loadTimeData} from '//resources/js/load_time_data.m.js';
import {WebUIListenerBehavior} from '//resources/js/web_ui_listener_behavior.m.js';
import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Route, Router} from '../../router.js';
import {DeepLinkingBehavior} from '../deep_linking_behavior.js';
import {routes} from '../os_route.js';
import {PrefsBehavior} from '../prefs_behavior.js';
import {RouteObserverBehavior} from '../route_observer_behavior.js';

import {CrostiniBrowserProxy, CrostiniBrowserProxyImpl, CrostiniDiskInfo, CrostiniPortActiveSetting, CrostiniPortProtocol, CrostiniPortSetting, DEFAULT_CROSTINI_CONTAINER, DEFAULT_CROSTINI_VM, MAX_VALID_PORT_NUMBER, MIN_VALID_PORT_NUMBER, PortState} from './crostini_browser_proxy.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'settings-crostini-page',

  behaviors: [
    DeepLinkingBehavior,
    I18nBehavior,
    PrefsBehavior,
    RouteObserverBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    /** @private {!Map<string, string>} */
    focusConfig_: {
      type: Object,
      value() {
        const map = new Map();
        if (routes.CROSTINI_DETAILS) {
          map.set(routes.CROSTINI_DETAILS.path, '#crostini .subpage-arrow');
        }
        if (routes.CROSTINI_DISK_RESIZE) {
          map.set(routes.CROSTINI_DISK_RESIZE.path, '#crostini .subpage-arrow');
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
              routes.CROSTINI_PORT_FORWARDING.path, '#crostini .subpage-arrow');
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
        return map;
      },
    },

    /**
     * Whether the install option should be enabled.
     * @private {boolean}
     */
    disableCrostiniInstall_: {
      type: Boolean,
    },

    /**
     * Used by DeepLinkingBehavior to focus this page's deep links.
     * @type {!Set<!chromeos.settings.mojom.Setting>}
     */
    supportedSettingIds: {
      type: Object,
      value: () => new Set([chromeos.settings.mojom.Setting.kSetUpCrostini]),
    },

    enableBruschetta_: {
      type: Boolean,
      value: loadTimeData.getBoolean('enableBruschetta'),
    }
  },

  attached() {
    if (!loadTimeData.getBoolean('allowCrostini')) {
      this.disableCrostiniInstall_ = true;
      return;
    }
    this.addWebUIListener(
        'crostini-installer-status-changed', (installerShowing) => {
          this.disableCrostiniInstall_ = installerShowing;
        });
    CrostiniBrowserProxyImpl.getInstance().requestCrostiniInstallerStatus();
  },

  /**
   * @param {!Route} route
   * @param {!Route} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== routes.CROSTINI) {
      return;
    }

    this.attemptDeepLink();
  },

  /**
   * @param {!Event} event
   * @private
   */
  onEnableTap_(event) {
    CrostiniBrowserProxyImpl.getInstance().requestCrostiniInstallerView();
    event.stopPropagation();
  },

  /** @private */
  onSubpageTap_(event) {
    // We do not open the subpage if the click was on a link.
    if (event.target && event.target.tagName === 'A') {
      event.stopPropagation();
      return;
    }

    if (this.getPref('crostini.enabled.value')) {
      Router.getInstance().navigateTo(routes.CROSTINI_DETAILS);
    }
  },

  /** @private */
  onBruschettaSubpageTap_(event) {
    if (this.enableBruschetta_) {
      Router.getInstance().navigateTo(routes.BRUSCHETTA_DETAILS);
    }
  },
});
