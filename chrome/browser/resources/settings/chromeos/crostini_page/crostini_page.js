// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'crostini-page' is the settings page for enabling Crostini.
 * Crostini Containers run Linux inside a Termina VM, allowing
 * the user to run Linux apps on their Chromebook.
 */

Polymer({
  is: 'settings-crostini-page',

  behaviors: [
    DeepLinkingBehavior,
    I18nBehavior,
    PrefsBehavior,
    settings.RouteObserverBehavior,
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
        if (settings.routes.CROSTINI_DETAILS) {
          map.set(
              settings.routes.CROSTINI_DETAILS.path,
              '#crostini .subpage-arrow');
        }
        if (settings.routes.CROSTINI_DISK_RESIZE) {
          map.set(
              settings.routes.CROSTINI_DISK_RESIZE.path,
              '#crostini .subpage-arrow');
        }
        if (settings.routes.CROSTINI_EXPORT_IMPORT) {
          map.set(
              settings.routes.CROSTINI_EXPORT_IMPORT.path,
              '#crostini .subpage-arrow');
        }
        if (settings.routes.CROSTINI_SHARED_PATHS) {
          map.set(
              settings.routes.CROSTINI_SHARED_PATHS.path,
              '#crostini .subpage-arrow');
        }
        if (settings.routes.CROSTINI_SHARED_USB_DEVICES) {
          map.set(
              settings.routes.CROSTINI_SHARED_USB_DEVICES.path,
              '#crostini .subpage-arrow');
        }
        if (settings.routes.CROSTINI_PORT_FORWARDING) {
          map.set(
              settings.routes.CROSTINI_PORT_FORWARDING.path,
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
    settings.CrostiniBrowserProxyImpl.getInstance()
        .requestCrostiniInstallerStatus();
  },

  /**
   * @param {!settings.Route} route
   * @param {!settings.Route} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== settings.routes.CROSTINI) {
      return;
    }

    this.attemptDeepLink();
  },

  /**
   * @param {!Event} event
   * @private
   */
  onEnableTap_(event) {
    settings.CrostiniBrowserProxyImpl.getInstance()
        .requestCrostiniInstallerView();
    event.stopPropagation();
  },

  /** @private */
  onSubpageTap_(event) {
    // We do not open the subpage if the click was on a link.
    if (event.target && event.target.tagName == 'A') {
      event.stopPropagation();
      return;
    }

    if (this.getPref('crostini.enabled.value')) {
      settings.Router.getInstance().navigateTo(
          settings.routes.CROSTINI_DETAILS);
    }
  },
});
