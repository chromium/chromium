// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'crostini-subpage' is the settings subpage for managing Crostini.
 */

/**
 * The current confirmation state.
 * @enum {string}
 */
const ConfirmationState = {
  NOT_CONFIRMED: 'notConfirmed',
  CONFIRMED: 'confirmed',
};

import {afterNextRender, Polymer, html, flush, Templatizer, TemplateInstanceBase} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import '//resources/cr_elements/cr_button/cr_button.m.js';
import '//resources/cr_elements/cr_link_row/cr_link_row.js';
import '//resources/cr_elements/icons.m.js';
import '../../controls/settings_toggle_button.js';

import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';
import {loadTimeData} from '//resources/js/load_time_data.m.js';
import {DEFAULT_CROSTINI_VM, DEFAULT_CROSTINI_CONTAINER, CrostiniPortProtocol, CrostiniPortSetting, CrostiniDiskInfo, CrostiniPortActiveSetting, CrostiniBrowserProxy, CrostiniBrowserProxyImpl, PortState, MIN_VALID_PORT_NUMBER, MAX_VALID_PORT_NUMBER} from './crostini_browser_proxy.js';
import './crostini_confirmation_dialog.js';
import {PrefsBehavior} from '../prefs_behavior.js';
import {DeepLinkingBehavior} from '../deep_linking_behavior.js';
import {routes} from '../os_route.m.js';
import {Router, Route} from '../../router.js';
import {RouteObserverBehavior} from '../route_observer_behavior.js';
import {RouteOriginBehaviorImpl, RouteOriginBehavior} from '../route_origin_behavior.js';
import {WebUIListenerBehavior} from '//resources/js/web_ui_listener_behavior.m.js';
import '../../settings_shared_css.js';
import {recordSettingChange, recordSearch, setUserActionRecorderForTesting, recordPageFocus, recordPageBlur, recordClick, recordNavigation} from '../metrics_recorder.m.js';
import './crostini_disk_resize_dialog.js';
import './crostini_disk_resize_confirmation_dialog.js';
import './crostini_port_forwarding.js';
import './crostini_extra_containers.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'settings-crostini-subpage',

  behaviors: [
    DeepLinkingBehavior,
    PrefsBehavior,
    RouteOriginBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * Whether export / import UI should be displayed.
     * @private {boolean}
     */
    showCrostiniExportImport_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('showCrostiniExportImport');
      },
    },

    /** @private {boolean} */
    showArcAdbSideloading_: {
      type: Boolean,
      computed: 'and_(isArcAdbSideloadingSupported_, isAndroidEnabled_)',
    },

    /** @private {boolean} */
    isArcAdbSideloadingSupported_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('arcAdbSideloadingSupported');
      },
    },

    /** @private {boolean} */
    showCrostiniPortForwarding_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('showCrostiniPortForwarding');
      },
    },

    /** @private {boolean} */
    showCrostiniExtraContainers_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('showCrostiniExtraContainers');
      },
    },

    /** @private {boolean} */
    isAndroidEnabled_: {
      type: Boolean,
    },

    /**
     * Whether the uninstall options should be displayed.
     * @private {boolean}
     */
    hideCrostiniUninstall_: {
      type: Boolean,
      computed: 'or_(installerShowing_, upgraderDialogShowing_)',
    },

    /**
     * Whether the button to launch the Crostini container upgrade flow should
     * be shown.
     * @private {boolean}
     */
    showCrostiniContainerUpgrade_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('showCrostiniContainerUpgrade');
      },
    },

    /**
     * Whether the button to show the disk resizing view should be shown.
     * @private {boolean}
     */
    showCrostiniDiskResize_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('showCrostiniDiskResize');
      },
    },

    /** @private */
    showDiskResizeConfirmationDialog_: {
      type: Boolean,
      value: false,
    },

    /*
     * Whether the installer is showing.
     * @private {boolean}
     */
    installerShowing_: {
      type: Boolean,
    },

    /**
     * Whether the upgrader dialog is showing.
     * @private {boolean}
     */
    upgraderDialogShowing_: {
      type: Boolean,
    },

    /**
     * Whether the button to launch the Crostini container upgrade flow should
     * be disabled.
     * @private {boolean}
     */
    disableUpgradeButton_: {
      type: Boolean,
      computed: 'or_(installerShowing_, upgraderDialogShowing_)',
    },

    /**
     * Whether the disk resizing dialog is visible or not
     * @private {boolean}
     */
    showDiskResizeDialog_: {
      type: Boolean,
      value: false,
    },

    /** @private {boolean} */
    showCrostiniMicPermissionDialog_: {
      type: Boolean,
      value: false,
    },

    /** @private {string} */
    diskSizeLabel_: {
      type: String,
      value: loadTimeData.getString('crostiniDiskSizeCalculating'),
    },

    /** @private {string} */
    diskResizeButtonLabel_: {
      type: String,
      value: loadTimeData.getString('crostiniDiskResizeShowButton'),
    },

    /** @private {string} */
    diskResizeButtonAriaLabel_: {
      type: String,
      value: loadTimeData.getString('crostiniDiskResizeShowButtonAriaLabel'),
    },

    /** @private {boolean} */
    canDiskResize_: {
      type: Boolean,
      value: false,
    },

    /**
     * Used by DeepLinkingBehavior to focus this page's deep links.
     * @type {!Set<!chromeos.settings.mojom.Setting>}
     */
    supportedSettingIds: {
      type: Object,
      value: () => new Set([
        chromeos.settings.mojom.Setting.kUninstallCrostini,
        chromeos.settings.mojom.Setting.kCrostiniDiskResize,
        chromeos.settings.mojom.Setting.kCrostiniMicAccess,
        chromeos.settings.mojom.Setting.kCrostiniContainerUpgrade,
      ]),
    },
  },

  /** RouteOriginBehavior override */
  route_: routes.CROSTINI_DETAILS,


  /** @private {boolean} */
  isDiskUserChosenSize_: false,

  /** @private {!ConfirmationState} */
  diskResizeConfirmationState_: ConfirmationState.NOT_CONFIRMED,

  observers: [
    'onCrostiniEnabledChanged_(prefs.crostini.enabled.value)',
    'onArcEnabledChanged_(prefs.arc.enabled.value)'
  ],

  attached() {
    this.addWebUIListener('crostini-installer-status-changed', (status) => {
      this.installerShowing_ = status;
    });
    this.addWebUIListener('crostini-upgrader-status-changed', (status) => {
      this.upgraderDialogShowing_ = status;
    });
    this.addWebUIListener(
        'crostini-container-upgrade-available-changed', (canUpgrade) => {
          this.showCrostiniContainerUpgrade_ = canUpgrade;
        });
    CrostiniBrowserProxyImpl.getInstance().requestCrostiniInstallerStatus();
    CrostiniBrowserProxyImpl.getInstance()
        .requestCrostiniUpgraderDialogStatus();
    CrostiniBrowserProxyImpl.getInstance()
        .requestCrostiniContainerUpgradeAvailable();
    this.loadDiskInfo_();
  },

  ready() {
    const r = routes;
    this.addFocusConfig(r.CROSTINI_SHARED_PATHS, '#crostini-shared-paths');
    this.addFocusConfig(
        r.CROSTINI_SHARED_USB_DEVICES, '#crostini-shared-usb-devices');
    this.addFocusConfig(r.CROSTINI_EXPORT_IMPORT, '#crostini-export-import');
    this.addFocusConfig(r.CROSTINI_ANDROID_ADB, '#crostini-enable-arc-adb');
    this.addFocusConfig(
        r.CROSTINI_PORT_FORWARDING, '#crostini-port-forwarding');
    this.addFocusConfig(
        r.CROSTINI_EXTRA_CONTAINERS, '#crostini-extra-containers');
  },

  /**
   * @param {!Route} route
   * @param {!Route} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== routes.CROSTINI_DETAILS) {
      return;
    }

    this.attemptDeepLink();
  },

  /** @private */
  onCrostiniEnabledChanged_(enabled) {
    if (!enabled &&
        Router.getInstance().getCurrentRoute() === routes.CROSTINI_DETAILS) {
      Router.getInstance().navigateToPreviousRoute();
    }
    if (enabled) {
      // The disk size or type could have changed due to the user reinstalling
      // Crostini, update our info.
      this.loadDiskInfo_();
    }
  },

  /** @private */
  onArcEnabledChanged_(enabled) {
    this.isAndroidEnabled_ = enabled;
  },

  /** @private */
  onExportImportClick_() {
    Router.getInstance().navigateTo(routes.CROSTINI_EXPORT_IMPORT);
  },

  /** @private */
  onEnableArcAdbClick_() {
    Router.getInstance().navigateTo(routes.CROSTINI_ANDROID_ADB);
  },

  /** @private */
  loadDiskInfo_() {
    // TODO(davidmunro): No magic 'termina' string.
    const vmName = 'termina';
    CrostiniBrowserProxyImpl.getInstance()
        .getCrostiniDiskInfo(vmName, /*requestFullInfo=*/ false)
        .then(
            diskInfo => {
              if (diskInfo.succeeded) {
                this.setResizeLabels_(diskInfo);
              }
            },
            reason => {
              console.log(`Unable to get info: ${reason}`);
            });
  },

  /**
   * @param {!CrostiniDiskInfo} diskInfo
   * @private
   */
  setResizeLabels_(diskInfo) {
    this.canDiskResize_ = diskInfo.canResize;
    if (!this.canDiskResize_) {
      this.diskSizeLabel_ =
          loadTimeData.getString('crostiniDiskResizeNotSupportedSubtext');
      return;
    }
    this.isDiskUserChosenSize_ = diskInfo.isUserChosenSize;
    if (this.isDiskUserChosenSize_) {
      if (diskInfo.ticks) {
        this.diskSizeLabel_ = diskInfo.ticks[diskInfo.defaultIndex].label;
      }
      this.diskResizeButtonLabel_ =
          loadTimeData.getString('crostiniDiskResizeShowButton');
      this.diskResizeButtonAriaLabel_ =
          loadTimeData.getString('crostiniDiskResizeShowButtonAriaLabel');
    } else {
      this.diskSizeLabel_ = loadTimeData.getString(
          'crostiniDiskResizeDynamicallyAllocatedSubtext');
      this.diskResizeButtonLabel_ =
          loadTimeData.getString('crostiniDiskReserveSizeButton');
      this.diskResizeButtonAriaLabel_ =
          loadTimeData.getString('crostiniDiskReserveSizeButtonAriaLabel');
    }
  },

  /** @private */
  onDiskResizeClick_() {
    if (!this.isDiskUserChosenSize_ &&
        this.diskResizeConfirmationState_ !== ConfirmationState.CONFIRMED) {
      this.showDiskResizeConfirmationDialog_ = true;
      return;
    }
    this.showDiskResizeDialog_ = true;
  },

  /** @private */
  onDiskResizeDialogClose_() {
    this.showDiskResizeDialog_ = false;
    this.diskResizeConfirmationState_ = ConfirmationState.NOT_CONFIRMED;
    // DiskInfo could have changed.
    this.loadDiskInfo_();
  },

  /** @private */
  onDiskResizeConfirmationDialogClose_() {
    // The on_cancel is followed by on_close, so check cancel didn't happen
    // first.
    if (this.showDiskResizeConfirmationDialog_) {
      this.diskResizeConfirmationState_ = ConfirmationState.CONFIRMED;
      this.showDiskResizeConfirmationDialog_ = false;
      this.showDiskResizeDialog_ = true;
    }
  },

  /** @private */
  onDiskResizeConfirmationDialogCancel_() {
    this.showDiskResizeConfirmationDialog_ = false;
  },

  /**
   * Shows a confirmation dialog when removing crostini.
   * @private
   */
  onRemoveClick_() {
    CrostiniBrowserProxyImpl.getInstance().requestRemoveCrostini();
    recordSettingChange();
  },

  /**
   * Shows the upgrade flow dialog.
   * @private
   */
  onContainerUpgradeClick_() {
    CrostiniBrowserProxyImpl.getInstance()
        .requestCrostiniContainerUpgradeView();
  },

  /** @private */
  onSharedPathsClick_() {
    Router.getInstance().navigateTo(routes.CROSTINI_SHARED_PATHS);
  },

  /** @private */
  onSharedUsbDevicesClick_() {
    Router.getInstance().navigateTo(routes.CROSTINI_SHARED_USB_DEVICES);
  },

  /** @private */
  onPortForwardingClick_() {
    Router.getInstance().navigateTo(routes.CROSTINI_PORT_FORWARDING);
  },

  /** @private */
  onExtraContainersClick_() {
    Router.getInstance().navigateTo(routes.CROSTINI_EXTRA_CONTAINERS);
  },

  /**
   * @private
   * @return {SettingsToggleButtonElement}
   */
  getMicToggle_() {
    return /** @type {SettingsToggleButtonElement} */ (
        this.$$('#crostini-mic-permission-toggle'));
  },

  /**
   * If a change to the mic settings requires Crostini to be restarted, a
   * dialog is shown.
   * @private
   */
  onMicPermissionChange_: async function() {
    if (await CrostiniBrowserProxyImpl.getInstance().checkCrostiniIsRunning()) {
      this.showCrostiniMicPermissionDialog_ = true;
    } else {
      this.getMicToggle_().sendPrefChange();
    }
  },

  /** @private */
  onCrostiniMicPermissionDialogClose_(e) {
    const toggle = this.getMicToggle_();
    if (e.detail.accepted) {
      toggle.sendPrefChange();
      CrostiniBrowserProxyImpl.getInstance().shutdownCrostini();
    } else {
      toggle.resetToPrefValue();
    }

    this.showCrostiniMicPermissionDialog_ = false;
  },

  /**
   * @private
   * @param {boolean} a
   * @param {boolean} b
   * @return {boolean}
   */
  and_: function(a, b) {
    return a && b;
  },

  /**
   * @private
   * @param {boolean} a
   * @param {boolean} b
   * @return {boolean}
   */
  or_: function(a, b) {
    return a || b;
  },
});
