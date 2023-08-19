// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import './mojo_api.js';
import './multidevice_setup_shared.css.js';
import './ui_page.js';
import '//resources/ash/common/cr.m.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/polymer/v3_0/iron-media-query/iron-media-query.js';
import 'chrome://resources/cros_components/lottie_renderer/lottie-renderer.js';

import {loadTimeData} from '//resources/ash/common/load_time_data.m.js';
import {WebUIListenerBehavior} from '//resources/ash/common/web_ui_listener_behavior.js';
import {Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {LottieRenderer} from 'chrome://resources/cros_components/lottie_renderer/lottie-renderer.js';
import {ConnectivityStatus} from 'chrome://resources/mojo/chromeos/ash/services/device_sync/public/mojom/device_sync.mojom-webui.js';
import {HostDevice} from 'chrome://resources/mojo/chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom-webui.js';

import {MojoInterfaceProvider, MojoInterfaceProviderImpl} from './mojo_api.js';
import {MultiDeviceSetupDelegate} from './multidevice_setup_delegate.js';
import {getTemplate} from './start_setup_page.html.js';
import {UiPageContainerBehavior} from './ui_page_container_behavior.js';

/**
 * The multidevice setup animation for light mode.
 * TODO(b/279667779): Remove when Jelly is fully launched.
 * @type {string}
 */
const MULTIDEVICE_ANIMATION_DARK_URL =
    'chrome://resources/ash/common/multidevice_setup/multidevice_setup_dark.json';

/**
 * The multidevice setup animation for dark mode.
 * TODO(b/279667779): Remove when Jelly is fully launched.
 * @type {string}
 */
const MULTIDEVICE_ANIMATION_LIGHT_URL =
    'chrome://resources/ash/common/multidevice_setup/multidevice_setup_light.json';

/**
 * The multidevice setup animation for dynamic colors.
 * @type {string}
 */
const MULTIDEVICE_ANIMATION_JELLY_URL =
    'chrome://resources/ash/common/multidevice_setup/multidevice_setup_jelly.json';

Polymer({
  _template: getTemplate(),
  is: 'start-setup-page',

  properties: {
    /* The localized loadTimeData string for the
     * StartSetupPage header text, dependent on whether
     * a user is on OOBE and previously connected their phone during Quick
     * Start.
     * */
    headerTextId: {
      type: String,
      value: 'startSetupPageHeader',
      notify: true,
    },

    /** Overridden from UiPageContainerBehavior. */
    forwardButtonTextId: {
      type: String,
      value: 'accept',
    },

    /** Overridden from UiPageContainerBehavior. */
    cancelButtonTextId: {
      type: String,
      computed: 'getCancelButtonTextId_(delegate)',
    },

    /**
     * Array of objects representing all potential MultiDevice hosts.
     *
     * @type {!Array<!HostDevice>}
     */
    devices: {
      type: Array,
      value: () => [],
      observer: 'devicesChanged_',
    },

    /**
     * Unique identifier for the currently selected host device. This uses the
     * device's Instance ID if it is available; otherwise, the device's legacy
     * device ID is used.
     * TODO(https://crbug.com/1019206): When v1 DeviceSync is turned off, only
     * use Instance ID since all devices are guaranteed to have one.
     *
     * Undefined if the no list of potential hosts has been received from mojo
     * service.
     *
     * @type {string|undefined}
     */
    selectedInstanceIdOrLegacyDeviceId: {
      type: String,
      notify: true,
    },

    /**
     * Delegate object which performs differently in OOBE vs. non-OOBE mode.
     * @type {!MultiDeviceSetupDelegate}
     */
    delegate: Object,

    /** @private */
    wifiSyncEnabled_: {
      type: Boolean,
      value() {
        return loadTimeData.valueExists('wifiSyncEnabled') &&
            loadTimeData.getBoolean('wifiSyncEnabled');
      },
    },

    /** @private */
    phoneHubCameraRollEnabled_: {
      type: Boolean,
      value() {
        return loadTimeData.valueExists('phoneHubCameraRollEnabled') &&
            loadTimeData.getBoolean('phoneHubCameraRollEnabled');
      },
    },

    /**
     * Whether the multidevice setup page is being rendered in dark mode.
     * TODO(b/279667779): Remove when Jelly is fully launched.
     * @private {boolean}
     */
    isDarkModeActive_: {
      type: Boolean,
      value: false,
    },

    /**
     * Whether the multidevice setup page is being rendered with dynamic colors.
     * @private {boolean}
     */
    isJellyEnabled_: {
      type: Boolean,
      value() {
        return loadTimeData.valueExists('isJellyEnabled') &&
            loadTimeData.getBoolean('isJellyEnabled');
      },
    },

    /**
     * ID of phone a user used to complete Quick Start earlier in OOBE flow.
     * @private {string|undefined}
     */
    quickStartPhoneInstanceId_: String,

    /**
     * Provider of an interface to the MultiDeviceSetup Mojo service.
     * @private {!MojoInterfaceProvider}
     */
    mojoInterfaceProvider_: Object,
  },

  behaviors: [
    UiPageContainerBehavior,
    WebUIListenerBehavior,
  ],

  /** @override */
  created() {
    this.mojoInterfaceProvider_ = MojoInterfaceProviderImpl.getInstance();
  },

  /** @override */
  attached() {
    this.addWebUIListener(
        'multidevice_setup.initializeSetupFlow',
        () => this.initializeSetupFlow_());

    this.addAccessibilityLabel_();
  },

  /**
   * This will play or stop the screen's lottie animation.
   * @param {boolean} enabled Whether the animation should play or not.
   */
  setPlayAnimation(enabled) {
    if (enabled) {
      this.$.multideviceSetupAnimation.play();
    } else {
      this.$.multideviceSetupAnimation.pause();
    }
  },

  /**
   * Since web links cannot be opened in OOBE as there is no web browser, this
   * attaches a listener to open a webview modal in OOBE when "Learn More" links
   * are clicked.
   * @private
   */
  initializeSetupFlow_() {
    // The "Learn More" links are inside a grdp string, so we cannot actually
    // add an onclick handler directly to the html. Instead, grab the two and
    // manaully add onclick handlers.
    const helpArticleLinks = [
      this.$$('#multidevice-summary-message a'),
    ];
    for (let i = 0; i < helpArticleLinks.length; i++) {
      helpArticleLinks[i].onclick = this.fire.bind(
          this, 'open-learn-more-webview-requested', helpArticleLinks[i].href);
    }

    this.mojoInterfaceProvider_.getMojoServiceRemote()
        .getQuickStartPhoneInstanceID()
        .then(({qsPhoneInstanceId}) => {
          if (!qsPhoneInstanceId) {
            return;
          }

          this.quickStartPhoneInstanceId_ = qsPhoneInstanceId;
        })
        .catch((error) => {
          console.warn('Mojo service failure: ' + error);
        });
  },

  /**
   * Adds ARIA description to "Learn More" links since the link tag is embedded
   * in the grdp string without additional attributes.
   * @private
   */
  addAccessibilityLabel_() {
    // Since the "Learn More" links are inside a grdp string, we add the
    // attribute here.
    const helpArticleLinks = [
      this.$$('#multidevice-summary-message a'),
    ];
    for (let i = 0; i < helpArticleLinks.length; i++) {
      helpArticleLinks[i].setAttribute(
          'aria-describedby', 'multidevice-summary-message');
    }
  },

  /**
   * @param {!MultiDeviceSetupDelegate} delegate
   * @return {string} The cancel button text ID, dependent on OOBE vs. non-OOBE.
   * @private
   */
  getCancelButtonTextId_(delegate) {
    return this.delegate.getStartSetupCancelButtonTextId();
  },

  /**
   * @param {!Array<!HostDevice>} devices
   * @return {string} Label for devices selection content.
   * @private
   */
  getDeviceSelectionHeader_(devices) {
    switch (devices.length) {
      case 0:
        return '';
      case 1:
        return this.i18n('startSetupPageSingleDeviceHeader');
      default:
        return this.i18n('startSetupPageMultipleDeviceHeader');
    }
  },

  /**
   * @param {!Array<!HostDevice>} devices
   * @return {boolean} True if there are more than one potential host devices.
   * @private
   */
  doesDeviceListHaveMultipleElements_(devices) {
    return devices.length > 1;
  },

  /**
   * @param {!Array<!HostDevice>} devices
   * @return {boolean} True if there is exactly one potential host device.
   * @private
   */
  doesDeviceListHaveOneElement_(devices) {
    return devices.length === 1;
  },

  /**
   * @param {!Array<!HostDevice>} devices
   * @return {string} Name of the first device in device list if there are any.
   *     Returns an empty string otherwise.
   * @private
   */
  getFirstDeviceNameInList_(devices) {
    return devices[0] ? this.devices[0].remoteDevice.deviceName : '';
  },

  /**
   * @param {!ConnectivityStatus} connectivityStatus
   * @return {string} The classes to bind to the device name option.
   * @private
   */
  getDeviceOptionClass_(connectivityStatus) {
    return connectivityStatus === ConnectivityStatus.kOffline ?
        'offline-device-name' :
        '';
  },

  /**
   * @param {!HostDevice} device
   * @return {string} Name of the device, with connectivity status information.
   * @private
   */
  getDeviceNameWithConnectivityStatus_(device) {
    return device.connectivityStatus === ConnectivityStatus.kOffline ?
        this.i18n(
            'startSetupPageOfflineDeviceOption',
            device.remoteDevice.deviceName) :
        device.remoteDevice.deviceName;
  },

  /**
   * @param {!HostDevice} device
   * @return {string} Returns a unique identifier for the input device, using
   *     the device's Instance ID if it is available; otherwise, the device's
   *     legacy device ID is used.
   *     TODO(https://crbug.com/1019206): When v1 DeviceSync is turned off, only
   *     use Instance ID since all devices are guaranteed to have one.
   * @private
   */
  getInstanceIdOrLegacyDeviceId_(device) {
    if (device.remoteDevice.instanceId) {
      return device.remoteDevice.instanceId;
    }

    return device.remoteDevice.deviceId;
  },

  /** @private */
  devicesChanged_() {
    if (this.devices.length > 0) {
      if (this.quickStartPhoneInstanceId_ &&
          this.moveDeviceToFront_(this.quickStartPhoneInstanceId_)) {
        // Adjust the title to reflect that the Quick Start phone was moved to
        // top of list.
        this.headerTextId = 'startSetupPageAfterQuickStartHeader';
      }

      this.selectedInstanceIdOrLegacyDeviceId =
          this.getInstanceIdOrLegacyDeviceId_(this.devices[0]);
    }
  },

  /**
   * Checks if the devices list contains a phone matching the provided
   * device_id. If so, that phone is moved to the front of the devices list.
   * @param {string} device_id
   * @return {boolean} Whether the device matching the provided ID is moved to
   *     the front of the devices list. Returns false if no matching device is
   *     found. Returns true and moves the matching device to index 0 if a
   *     matching device is found.
   * @private
   */
  moveDeviceToFront_(device_id) {
    const matchingDeviceIdx = this.devices.findIndex(
        device => this.getInstanceIdOrLegacyDeviceId_(device) === device_id);

    if (matchingDeviceIdx === -1) {
      return false;
    }

    // Move device located at the matchingDeviceIdx to the front of the
    // devices list.
    this.devices.unshift(this.devices.splice(matchingDeviceIdx, 1)[0]);
    return true;
  },

  /** @private */
  onDeviceDropdownSelectionChanged_() {
    this.selectedInstanceIdOrLegacyDeviceId = this.$.deviceDropdown.value;
  },

  /**
   * Wrapper for i18nAdvanced for binding to location updates in OOBE.
   * @param {string} locale The language code (e.g. en, es) for the current
   *     display language for CrOS. As with I18nBehavior.i18nDynamic(), the
   *     parameter is not used directly but is provided to allow HTML binding
   *     without passing an unexpected argument to I18nBehavior.i18nAdvanced().
   * @param {string} textId The loadTimeData ID of the string to be translated.
   * @private
   */
  i18nAdvancedDynamic_(locale, textId) {
    return this.i18nAdvanced(textId);
  },

  /**
   * Returns the URL for the asset that defines the multidevice setup page's
   * animation
   * @return {string}
   * @private
   */
  getAnimationUrl_() {
    if (this.isJellyEnabled_) {
      return MULTIDEVICE_ANIMATION_JELLY_URL;
    }

    // TODO(b/279667779): Remove when Jelly is fully launched.
    return this.isDarkModeActive_ ? MULTIDEVICE_ANIMATION_DARK_URL :
                                    MULTIDEVICE_ANIMATION_LIGHT_URL;
  },
});
