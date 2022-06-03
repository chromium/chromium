// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('mobile', function() {

  function MobileSetup() {}

  cr.addSingletonGetter(MobileSetup);

  MobileSetup.PLAN_ACTIVATION_UNKNOWN = -2;
  MobileSetup.PLAN_ACTIVATION_PAGE_LOADING = -1;
  MobileSetup.PLAN_ACTIVATION_START = 0;
  MobileSetup.PLAN_ACTIVATION_TRYING_OTASP = 1;
  MobileSetup.PLAN_ACTIVATION_INITIATING_ACTIVATION = 3;
  MobileSetup.PLAN_ACTIVATION_RECONNECTING = 4;
  MobileSetup.PLAN_ACTIVATION_WAITING_FOR_CONNECTION = 5;
  MobileSetup.PLAN_ACTIVATION_PAYMENT_PORTAL_LOADING = 6;
  MobileSetup.PLAN_ACTIVATION_SHOWING_PAYMENT = 7;
  MobileSetup.PLAN_ACTIVATION_RECONNECTING_PAYMENT = 8;
  MobileSetup.PLAN_ACTIVATION_DELAY_OTASP = 9;
  MobileSetup.PLAN_ACTIVATION_START_OTASP = 10;
  MobileSetup.PLAN_ACTIVATION_OTASP = 11;
  MobileSetup.PLAN_ACTIVATION_DONE = 12;
  MobileSetup.PLAN_ACTIVATION_ERROR = 0xFF;

  MobileSetup.EXTENSION_PAGE_URL =
      'chrome-extension://iadeocfgjdjdmpenejdbfeaocpbikmab';
  MobileSetup.ACTIVATION_PAGE_URL =
      MobileSetup.EXTENSION_PAGE_URL + '/activation.html';
  MobileSetup.PORTAL_OFFLINE_PAGE_URL =
      MobileSetup.EXTENSION_PAGE_URL + '/portal_offline.html';

  MobileSetup.prototype = {
    // Mobile device information.
    deviceInfo_: null,
    initialized_: false,
    fakedTransaction_: false,
    paymentShown_: false,
    carrierPageUrl_: null,
    spinnerInt_: -1,
    // UI states.
    state_: MobileSetup.PLAN_ACTIVATION_UNKNOWN,
    STATE_UNKNOWN_: 'unknown',
    STATE_CONNECTING_: 'connecting',
    STATE_ERROR_: 'error',
    STATE_PAYMENT_: 'payment',
    STATE_ACTIVATING_: 'activating',
    STATE_CONNECTED_: 'connected',

    initialize(frame_name, carrierPage) {
      if (this.initialized_) {
        console.log('calling initialize() again?');
        return;
      }
      this.initialized_ = true;
      self = this;

      cr.ui.dialogs.BaseDialog.OK_LABEL = loadTimeData.getString('ok_button');
      cr.ui.dialogs.BaseDialog.CANCEL_LABEL =
          loadTimeData.getString('cancel_button');
      this.confirm_ = new cr.ui.dialogs.ConfirmDialog(document.body);

      window.addEventListener('message', function(e) {
        self.onMessageReceived_(e);
      });

      $('closeButton').addEventListener('click', function(e) {
        $('finalStatus').classList.add('hidden');
      });

      // Kick off activation process.
      chrome.send('startActivation');
    },

    startSpinner_() {
      this.stopSpinner_();
      this.spinnerInt_ = setInterval(mobile.MobileSetup.drawProgress, 100);
    },

    stopSpinner_() {
      if (this.spinnerInt_ != -1) {
        clearInterval(this.spinnerInt_);
        this.spinnerInt_ = -1;
      }
    },

    /**
     * Handler for loadabort event on the payment portal webview.
     * Notifies Chrome that the payment portal load failed.
     * @param {string} paymentUrl The payment portal URL, as provided by the
     *     cellular service.
     * @param {!Object} evt Load abort event.
     * @private
     */
    paymentLoadAborted_(paymentUrl, evt) {
      if (!evt.isTopLevel ||
          new URL(evt.url).origin != new URL(paymentUrl).origin) {
        return;
      }
      chrome.send('paymentPortalLoad', ['failed']);
    },

    /**
     * Handler for loadcommit event on the payment portal webview.
     * Notifies Chrome that the payment portal was loaded.
     * @param {string} paymentUrl The payment portal URL, as provided by the
     *     cellular service.
     * @param {!Object} evt Load commit event.
     * @private
     */
    paymentLoadCommitted_(paymentUrl, evt) {
      if (!evt.isTopLevel ||
          new URL(evt.url).origin != new URL(paymentUrl).origin) {
        return;
      }

      // Workaround for https://crbug.com/893248 - for some reason, the payment
      // portal does not load properly after the payment frame submition.
      // Reloading the frame seems to bypass the problem.
      // TODO(tbarzic): Remove this once the problem has been properly
      //     addressed.
      if (!this.paymentPortalReloaded_) {
        this.paymentPortalReloaded_ = true;
        $('portalFrameWebview').reload();
      } else {
        chrome.send('paymentPortalLoad', ['ok']);
      }
    },

    /**
     * Sends a <code>loadedInWebview</code> message to the mobile service
     * portal webview.
     * @param {string} paymentUrl The payment portal URL - used to restrict
     *     origins to which the message is sent.
     */
    sendInitialMessage_(paymentUrl) {
      $('portalFrameWebview')
          .contentWindow.postMessage({msg: 'loadedInWebview'}, paymentUrl);
    },

    /**
     * Loads payment URL defined by <code>deviceInfo</code> into the
     * portal frame webview.
     * If the webview element already exists, it will not be reused - the
     * existing webview will be removed from DOM, and a new one will be
     * created.
     * If deviceInfo provides post data to be sent to the payment URL, the
     * webview will be initilized using
     * <code>webviewPost.util.postDeviceDataToWebview</code>, otherwise the
     * payment URL will be loaded directly into the webview.
     *
     * Note that the portal frame webview will only ever contain data and web
     * URLs - it will never embed the mobile setup extension resources.
     *
     * @param {!Object} deviceInfo The cellular service info - contains the
     *     information that should be passed to the payment portal.
     * @private
     */
    loadPaymentFrame_(deviceInfo) {
      if (!deviceInfo)
        return;
      this.deviceInfo_ = deviceInfo;

      var existingWebview = $('portalFrameWebview');
      if (existingWebview)
        existingWebview.remove();

      var frame = document.createElement('webview');
      frame.id = 'portalFrameWebview';

      this.paymentPortalReloaded_ = false;

      $('portalFrame').appendChild(frame);

      frame.addEventListener(
          'loadabort',
          this.paymentLoadAborted_.bind(this, deviceInfo.payment_url));

      frame.addEventListener(
          'loadcommit',
          this.paymentLoadCommitted_.bind(this, deviceInfo.payment_url));

      // Send a message to the loaded webview, so it can get a reference to
      // which to send messages as needed.
      frame.addEventListener(
          'loadstop',
          this.sendInitialMessage_.bind(this, deviceInfo.payment_url));

      if (deviceInfo.post_data && deviceInfo.post_data.length) {
        webviewPost.util.postDeviceDataToWebview(
            frame, deviceInfo.payment_url, deviceInfo.post_data);
      } else {
        frame.src = deviceInfo.payment_url;
      }
    },

    onMessageReceived_(e) {
      if (e.origin !=
          this.deviceInfo_.payment_url.substring(0, e.origin.length))
        return;

      if (e.data.type == 'requestDeviceInfoMsg') {
        this.sendDeviceInfo_();
      } else if (e.data.type == 'reportTransactionStatusMsg') {
        console.log('calling setTransactionStatus from onMessageReceived_');
        chrome.send('setTransactionStatus', [e.data.status]);
      }
    },

    changeState_(deviceInfo) {
      var newState = deviceInfo.state;
      if (this.state_ == newState)
        return;

      // The mobile setup is already in its final state.
      if (this.state_ == MobileSetup.PLAN_ACTIVATION_DONE ||
          this.state_ == MobileSetup.PLAN_ACTIVATION_ERROR) {
        return;
      }

      // Map handler state to UX.
      var simpleActivationFlow =
          (deviceInfo.activation_type == 'NonCellular' ||
           deviceInfo.activation_type == 'OTA');
      switch (newState) {
        case MobileSetup.PLAN_ACTIVATION_PAGE_LOADING:
        case MobileSetup.PLAN_ACTIVATION_START:
        case MobileSetup.PLAN_ACTIVATION_DELAY_OTASP:
        case MobileSetup.PLAN_ACTIVATION_START_OTASP:
        case MobileSetup.PLAN_ACTIVATION_RECONNECTING:
        case MobileSetup.PLAN_ACTIVATION_RECONNECTING_PAYMENT:
          // Activation page should not be shown for the simple activation
          // flow.
          if (simpleActivationFlow)
            break;

          $('statusHeader').textContent =
              loadTimeData.getString('connecting_header');
          $('auxHeader').textContent = loadTimeData.getString('please_wait');
          $('portalFrame').classList.add('hidden');
          $('finalStatus').classList.add('hidden');
          this.setCarrierPage_(MobileSetup.ACTIVATION_PAGE_URL);
          $('systemStatus').classList.remove('hidden');
          $('canvas').classList.remove('hidden');
          this.startSpinner_();
          break;
        case MobileSetup.PLAN_ACTIVATION_TRYING_OTASP:
        case MobileSetup.PLAN_ACTIVATION_INITIATING_ACTIVATION:
        case MobileSetup.PLAN_ACTIVATION_OTASP:
          // Activation page should not be shown for the simple activation
          // flow.
          if (simpleActivationFlow)
            break;

          $('statusHeader').textContent =
              loadTimeData.getString('activating_header');
          $('auxHeader').textContent = loadTimeData.getString('please_wait');
          $('portalFrame').classList.add('hidden');
          $('finalStatus').classList.add('hidden');
          this.setCarrierPage_(MobileSetup.ACTIVATION_PAGE_URL);
          $('systemStatus').classList.remove('hidden');
          $('canvas').classList.remove('hidden');
          this.startSpinner_();
          break;
        case MobileSetup.PLAN_ACTIVATION_PAYMENT_PORTAL_LOADING:
          // Activation page should not be shown for the simple activation
          // flow.
          if (!simpleActivationFlow) {
            $('statusHeader').textContent =
                loadTimeData.getString('connecting_header');
            $('auxHeader').textContent = '';
            $('portalFrame').classList.add('hidden');
            $('finalStatus').classList.add('hidden');
            this.setCarrierPage_(MobileSetup.ACTIVATION_PAGE_URL);
            $('systemStatus').classList.remove('hidden');
            $('canvas').classList.remove('hidden');
          }
          this.loadPaymentFrame_(deviceInfo);
          break;
        case MobileSetup.PLAN_ACTIVATION_WAITING_FOR_CONNECTION:
          var statusHeaderText;
          var carrierPage;
          if (deviceInfo.activation_type == 'NonCellular') {
            statusHeaderText =
                loadTimeData.getString('portal_unreachable_header');
            carrierPage = MobileSetup.PORTAL_OFFLINE_PAGE_URL;
          } else if (deviceInfo.activation_type == 'OTA') {
            statusHeaderText = loadTimeData.getString('connecting_header');
            carrierPage = MobileSetup.ACTIVATION_PAGE_URL;
          }
          $('statusHeader').textContent = statusHeaderText;
          $('auxHeader').textContent = '';
          $('auxHeader').classList.add('hidden');
          $('portalFrame').classList.add('hidden');
          $('finalStatus').classList.add('hidden');
          $('systemStatus').classList.remove('hidden');
          this.setCarrierPage_(carrierPage);
          $('canvas').classList.remove('hidden');
          this.startSpinner_();
          break;
        case MobileSetup.PLAN_ACTIVATION_SHOWING_PAYMENT:
          $('statusHeader').textContent = '';
          $('auxHeader').textContent = '';
          $('finalStatus').classList.add('hidden');
          $('systemStatus').classList.add('hidden');
          $('portalFrame').classList.remove('hidden');
          $('canvas').classList.add('hidden');
          this.stopSpinner_();
          this.paymentShown_ = true;
          break;
        case MobileSetup.PLAN_ACTIVATION_DONE:
          $('statusHeader').textContent = '';
          $('auxHeader').textContent = '';
          $('finalHeader').textContent =
              loadTimeData.getString('completed_header');
          $('finalMessage').textContent =
              loadTimeData.getString('completed_text');
          $('systemStatus').classList.add('hidden');
          $('closeButton').classList.remove('hidden');
          $('finalStatus').classList.remove('hidden');
          $('canvas').classList.add('hidden');
          $('closeButton').classList.toggle('hidden', !this.paymentShown_);
          $('portalFrame').classList.toggle('hidden', !this.paymentShown_);
          this.stopSpinner_();
          break;
        case MobileSetup.PLAN_ACTIVATION_ERROR:
          $('statusHeader').textContent = '';
          $('auxHeader').textContent = '';
          $('finalHeader').textContent = loadTimeData.getString('error_header');
          $('finalMessage').textContent = deviceInfo.error;
          $('systemStatus').classList.add('hidden');
          $('canvas').classList.add('hidden');
          $('closeButton').classList.toggle('hidden', !this.paymentShown_);
          $('portalFrame').classList.toggle('hidden', !this.paymentShown_);
          $('finalStatus').classList.remove('hidden');
          this.stopSpinner_();
          break;
      }
      this.state_ = newState;
    },

    /**
     * Embeds a URL into the <code>carrierPage</code> webview. The webview is
     * ever expected to contain mobile setup extension URLs.
     * @param {string} url The URL to embed into the carrierPage webview.
     * @param
     */
    setCarrierPage_(url) {
      if (this.carrierPageUrl_ == url)
        return;

      this.carrierPageUrl_ = url;
      $('carrierPage').src = url;
    },

    updateDeviceStatus_(deviceInfo) {
      this.changeState_(deviceInfo);
    },

    sendDeviceInfo_() {
      var msg = {
        type: 'deviceInfoMsg',
        domain: document.location,
        payload: {
          'carrier': this.deviceInfo_.carrier,
          'MEID': this.deviceInfo_.MEID,
          'IMEI': this.deviceInfo_.IMEI,
          'MDN': this.deviceInfo_.MDN
        }
      };
      $('portalFrameWebview')
          .contentWindow.postMessage(msg, this.deviceInfo_.payment_url);
    }

  };

  MobileSetup.drawProgress = function() {
    var ctx = canvas.getContext('2d');
    ctx.clearRect(0, 0, canvas.width, canvas.height);

    var segmentCount = Math.min(12, canvas.width / 1.6);  // Number of segments
    var rotation = 0.75;  // Counterclockwise rotation

    // Rotate canvas over time
    ctx.translate(canvas.width / 2, canvas.height / 2);
    ctx.rotate(Math.PI * 2 / (segmentCount + rotation));
    ctx.translate(-canvas.width / 2, -canvas.height / 2);

    var gap = canvas.width / 24;                 // Gap between segments
    var oRadius = canvas.width / 2;              // Outer radius
    var iRadius = oRadius * 0.618;               // Inner radius
    var oCircumference = Math.PI * 2 * oRadius;  // Outer circumference
    var iCircumference = Math.PI * 2 * iRadius;  // Inner circumference
    var oGap = gap / oCircumference;  // Gap size as fraction of  outer ring
    var iGap = gap / iCircumference;  // Gap size as fraction of  inner ring
    var oArc = Math.PI * 2 * (1 / segmentCount - oGap);  // Angle of outer arcs
    var iArc = Math.PI * 2 * (1 / segmentCount - iGap);  // Angle of inner arcs

    for (i = 0; i < segmentCount; i++) {  // Draw each segment
      var opacity = Math.pow(1.0 - i / segmentCount, 3.0);
      opacity = (0.15 + opacity * 0.8);  // Vary from 0.15 to 0.95
      var angle = -Math.PI * 2 * i / segmentCount;

      ctx.beginPath();
      ctx.arc(
          canvas.width / 2, canvas.height / 2, oRadius, angle - oArc / 2,
          angle + oArc / 2, false);
      ctx.arc(
          canvas.width / 2, canvas.height / 2, iRadius, angle + iArc / 2,
          angle - iArc / 2, true);
      ctx.closePath();
      ctx.fillStyle = 'rgba(240, 30, 29, ' + opacity + ')';
      ctx.fill();
    }
  };

  MobileSetup.deviceStateChanged = function(deviceInfo) {
    MobileSetup.getInstance().updateDeviceStatus_(deviceInfo);
  };

  MobileSetup.loadPage = function() {
    mobile.MobileSetup.getInstance().initialize(
        mobile.MobileSetup.ACTIVATION_PAGE_URL);
  };

  // Export
  return {MobileSetup: MobileSetup};
});

document.addEventListener('DOMContentLoaded', mobile.MobileSetup.loadPage);
