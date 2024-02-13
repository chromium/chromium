// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('mobile', function() {
  /** @enum {number} */
  const NetworkState = {UNKNOWN: 0, PORTAL_REACHABLE: 1, PORTAL_UNREACHABLE: 2};

  /** @enum {number} */
  const StatusMessageType = {NOT_SET: 0, PORTAL_OFFLINE: 1};

  function PortalImpl() {
    // Mobile device information.
    this.deviceInfo_ = null;
    this.networkState_ = NetworkState.UNKNOWN;
    this.portalFrameSet_ = false;
    this.statusMessageType_ = StatusMessageType.NOT_SET;
  }

  /** @type {?PortalImpl} */
  let instance = null;

  /** @return {!PortalImpl} */
  PortalImpl.getInstance = function() {
    return instance || (instance = new PortalImpl());
  };

  PortalImpl.prototype = {
    initialize() {
      // Get network device info for which portal should be opened.
      // For LTE networks, this will also start observing network connection
      // state and raise |updatePortalReachability| messages when the portal
      // reachability changes.
      chrome.send('getDeviceInfo');
    },

    updateDeviceInfo(deviceInfo) {
      this.deviceInfo_ = deviceInfo;
      this.updateState_();
    },

    updateNetworkState(networkState) {
      if (this.networkState_ === networkState) {
        return;
      }
      this.networkState_ = networkState;

      // If the device info is not yet set, the state will be updated on the
      // device info update.
      if (this.deviceInfo_) {
        this.updateState_();
      }
    },

    updateState_() {
      if (!this.deviceInfo_ || this.networkState_ === NetworkState.UNKNOWN) {
        return;
      }

      if (!this.isDeviceInfoValid_() ||
          this.networkState_ !== NetworkState.PORTAL_REACHABLE) {
        // If the device info is not valid or portal is unreachable, hide
        // portalFrame and show system status displaying error message.
        this.setStatusMessage_(StatusMessageType.PORTAL_OFFLINE);
        $('portalFrame').hidden = true;
        $('systemStatus').hidden = false;
      } else {
        // If the portal is reachable and device info is valid, set and show
        // portalFrame; and hide system status displaying 'offline portal'
        // message.
        this.setPortalFrameIfNeeded_(this.deviceInfo_);
        this.setStatusMessage_(StatusMessageType.NOT_SET);
        $('portalFrame').hidden = false;
        $('systemStatus').hidden = true;
      }
    },

    /**
     * Updates the status header and status message text content, depending on
     * the requested status message type.
     * @param {StatusMessageType} type The requested status message type.
     * @private
     */
    setStatusMessage_(type) {
      // The status is already set, nothing to do.
      if (type === this.statusMessageType_) {
        return;
      }

      switch (type) {
        case StatusMessageType.PORTAL_OFFLINE:
          $('statusHeader').textContent =
              loadTimeData.getString('view_account_error_title');
          $('statusMessage').textContent =
              loadTimeData.getString('view_account_error_message');
          break;
        case StatusMessageType.NOT_SET:
          $('statusMessage').textContent = '';
          $('statusHeader').textContent = '';
          break;
        default:
          break;
      }

      this.statusMessageType_ = type;
    },

    /**
     * Initilizes payment portal webview using payment URL and post data
     * provided by the cellular service associated with this web UI.
     * The portal webview is initilized only once, and is expected to load only
     * data and web URLs - it should never load mobile setup extension URLs.
     * @param {!Object} deviceInfo Information about the cellular service for
     *     which the portal should be loaded (the relevant information is the
     *     network's payment URL and POST request data).
     * @private
     */
    setPortalFrameIfNeeded_(deviceInfo) {
      // The portal should be set only once.
      if (this.portalFrameSet_) {
        return;
      }

      webviewPost.util.postDeviceDataToWebview(
          $('portalFrame'), deviceInfo.payment_url, deviceInfo.post_data);

      this.portalFrameSet_ = true;
    },

    isDeviceInfoValid_() {
      // Device info is valid if it has mdn which doesn't contain only '0's.
      return this.deviceInfo_ && this.deviceInfo_.MDN &&
          this.deviceInfo_.MDN.match('[^0]');
    },

  };

  function MobileSetupPortal() {}

  MobileSetupPortal.loadPage = function() {
    PortalImpl.getInstance().initialize();
  };

  MobileSetupPortal.onGotDeviceInfo = function(deviceInfo) {
    PortalImpl.getInstance().updateDeviceInfo(deviceInfo);
  };

  MobileSetupPortal.onConnectivityChanged = function(portalReachable) {
    PortalImpl.getInstance().updateNetworkState(
        portalReachable ? NetworkState.PORTAL_REACHABLE :
                          NetworkState.PORTAL_UNREACHABLE);
  };

  // Export
  return {MobileSetupPortal: MobileSetupPortal};
});

document.addEventListener(
    'DOMContentLoaded', mobile.MobileSetupPortal.loadPage);
