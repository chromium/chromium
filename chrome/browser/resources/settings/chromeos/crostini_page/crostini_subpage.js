// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'crostini-subpage' is the settings subpage for managing Crostini.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/icons.m.js';
import '../../controls/settings_toggle_button.js';
import './crostini_confirmation_dialog.js';
import '../../settings_shared.css.js';
import './crostini_disk_resize_dialog.js';
import './crostini_disk_resize_confirmation_dialog.js';
import './crostini_port_forwarding.js';
import './crostini_extra_containers.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Setting} from '../../mojom-webui/setting.mojom-webui.js';
import {Route, Router} from '../../router.js';
import {DeepLinkingBehavior, DeepLinkingBehaviorInterface} from '../deep_linking_behavior.js';
import {recordSettingChange} from '../metrics_recorder.js';
import {routes} from '../os_route.js';
import {PrefsBehavior, PrefsBehaviorInterface} from '../prefs_behavior.js';
import {RouteOriginBehavior, RouteOriginBehaviorImpl, RouteOriginBehaviorInterface} from '../route_origin_behavior.js';

import {CrostiniBrowserProxy, CrostiniBrowserProxyImpl, CrostiniDiskInfo} from './crostini_browser_proxy.js';

/**
 * The current confirmation state.
 * @enum {string}
 */
const ConfirmationState = {
  NOT_CONFIRMED: 'notConfirmed',
  CONFIRMED: 'confirmed',
};

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {DeepLinkingBehaviorInterface}
 * @implements {PrefsBehaviorInterface}
 * @implements {RouteOriginBehaviorInterface}
 * @implements {WebUIListenerBehaviorInterface}
 */
const SettingsCrostiniSubpageElementBase = mixinBehaviors(
    [
      DeepLinkingBehavior,
      PrefsBehavior,
      RouteOriginBehavior,
      WebUIListenerBehavior,
    ],
    PolymerElement);

/** @polymer */
class SettingsCrostiniSubpageElement extends
    SettingsCrostiniSubpageElementBase {
  static get is() {
    return 'settings-crostini-subpage';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
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
       * @type {!Set<!Setting>}
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set([
          Setting.kUninstallCrostini,
          Setting.kCrostiniDiskResize,
          Setting.kCrostiniMicAccess,
          Setting.kCrostiniContainerUpgrade,
        ]),
      },
    };
  }

  static get observers() {
    return [
      'onCrostiniEnabledChanged_(prefs.crostini.enabled.value)',
      'onArcEnabledChanged_(prefs.arc.enabled.value)',
    ];
  }

  constructor() {
    super();

    /** RouteOriginBehavior override */
    this.route_ = routes.CROSTINI_DETAILS;

    /** @private {boolean} */
    this.isDiskUserChosenSize_ = false;

    /** @private {!ConfirmationState} */
    this.diskResizeConfirmationState_ = ConfirmationState.NOT_CONFIRMED;

    /** @private {!CrostiniBrowserProxy} */
    this.browserProxy_ = CrostiniBrowserProxyImpl.getInstance();
  }

  connectedCallback() {
    super.connectedCallback();

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
    this.browserProxy_.requestCrostiniInstallerStatus();
    this.browserProxy_.requestCrostiniUpgraderDialogStatus();
    this.browserProxy_.requestCrostiniContainerUpgradeAvailable();
    this.loadDiskInfo_();
  }

  ready() {
    super.ready();

    const r = routes;
    this.addFocusConfig(r.CROSTINI_SHARED_PATHS, '#crostini-shared-paths');
    this.addFocusConfig(
        r.CROSTINI_SHARED_USB_DEVICES, '#crostini-shared-usb-devices');
    this.addFocusConfig(
        r.BRUSCHETTA_SHARED_USB_DEVICES, '#bruschetta-shared-usb-devices');
    this.addFocusConfig(r.CROSTINI_EXPORT_IMPORT, '#crostini-export-import');
    this.addFocusConfig(r.CROSTINI_ANDROID_ADB, '#crostini-enable-arc-adb');
    this.addFocusConfig(
        r.CROSTINI_PORT_FORWARDING, '#crostini-port-forwarding');
    this.addFocusConfig(
        r.CROSTINI_EXTRA_CONTAINERS, '#crostini-extra-containers');
  }

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
  }

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
  }

  /** @private */
  onArcEnabledChanged_(enabled) {
    this.isAndroidEnabled_ = enabled;
  }

  /** @private */
  onExportImportClick_() {
    Router.getInstance().navigateTo(routes.CROSTINI_EXPORT_IMPORT);
  }

  /** @private */
  onEnableArcAdbClick_() {
    Router.getInstance().navigateTo(routes.CROSTINI_ANDROID_ADB);
  }

  /** @private */
  loadDiskInfo_() {
    // TODO(davidmunro): No magic 'termina' string.
    const vmName = 'termina';
    this.browserProxy_.getCrostiniDiskInfo(vmName, /*requestFullInfo=*/ false)
        .then(
            diskInfo => {
              if (diskInfo.succeeded) {
                this.setResizeLabels_(diskInfo);
              }
            },
            reason => {
              console.warn(`Unable to get info: ${reason}`);
            });
  }

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
  }

  /** @private */
  onDiskResizeClick_() {
    if (!this.isDiskUserChosenSize_ &&
        this.diskResizeConfirmationState_ !== ConfirmationState.CONFIRMED) {
      this.showDiskResizeConfirmationDialog_ = true;
      return;
    }
    this.showDiskResizeDialog_ = true;
  }

  /** @private */
  onDiskResizeDialogClose_() {
    this.showDiskResizeDialog_ = false;
    this.diskResizeConfirmationState_ = ConfirmationState.NOT_CONFIRMED;
    // DiskInfo could have changed.
    this.loadDiskInfo_();
  }

  /** @private */
  onDiskResizeConfirmationDialogClose_() {
    // The on_cancel is followed by on_close, so check cancel didn't happen
    // first.
    if (this.showDiskResizeConfirmationDialog_) {
      this.diskResizeConfirmationState_ = ConfirmationState.CONFIRMED;
      this.showDiskResizeConfirmationDialog_ = false;
      this.showDiskResizeDialog_ = true;
    }
  }

  /** @private */
  onDiskResizeConfirmationDialogCancel_() {
    this.showDiskResizeConfirmationDialog_ = false;
  }

  /**
   * Shows a confirmation dialog when removing crostini.
   * @private
   */
  onRemoveClick_() {
    this.browserProxy_.requestRemoveCrostini();
    recordSettingChange();
  }

  /**
   * Shows the upgrade flow dialog.
   * @private
   */
  onContainerUpgradeClick_() {
    this.browserProxy_.requestCrostiniContainerUpgradeView();
  }

  /** @private */
  onSharedPathsClick_() {
    Router.getInstance().navigateTo(routes.CROSTINI_SHARED_PATHS);
  }

  /** @private */
  onSharedUsbDevicesClick_() {
    Router.getInstance().navigateTo(routes.CROSTINI_SHARED_USB_DEVICES);
  }

  /** @private */
  onBruschettaSharedUsbDevicesClick_() {
    Router.getInstance().navigateTo(routes.BRUSCHETTA_SHARED_USB_DEVICES);
  }

  /** @private */
  onPortForwardingClick_() {
    Router.getInstance().navigateTo(routes.CROSTINI_PORT_FORWARDING);
  }

  /** @private */
  onExtraContainersClick_() {
    Router.getInstance().navigateTo(routes.CROSTINI_EXTRA_CONTAINERS);
  }

  /**
   * @private
   * @return {SettingsToggleButtonElement}
   */
  getMicToggle_() {
    return /** @type {SettingsToggleButtonElement} */ (
        this.shadowRoot.querySelector('#crostini-mic-permission-toggle'));
  }

  /**
   * If a change to the mic settings requires Crostini to be restarted, a
   * dialog is shown.
   * @private
   */
  async onMicPermissionChange_() {
    if (await this.browserProxy_.checkCrostiniIsRunning()) {
      this.showCrostiniMicPermissionDialog_ = true;
    } else {
      this.getMicToggle_().sendPrefChange();
    }
  }

  /** @private */
  onCrostiniMicPermissionDialogClose_(e) {
    const toggle = this.getMicToggle_();
    if (e.detail.accepted) {
      toggle.sendPrefChange();
      this.browserProxy_.shutdownCrostini();
    } else {
      toggle.resetToPrefValue();
    }

    this.showCrostiniMicPermissionDialog_ = false;
  }

  /**
   * @private
   * @param {boolean} a
   * @param {boolean} b
   * @return {boolean}
   */
  and_(a, b) {
    return a && b;
  }

  /**
   * @private
   * @param {boolean} a
   * @param {boolean} b
   * @return {boolean}
   */
  or_(a, b) {
    return a || b;
  }
}

customElements.define(
    SettingsCrostiniSubpageElement.is, SettingsCrostiniSubpageElement);
