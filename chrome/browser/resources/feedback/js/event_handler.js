// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <include src="feedback_util.js">

/**
 * @type {number}
 * @const
 */
const FEEDBACK_WIDTH = 500;
/**
 * @type {number}
 * @const
 */
const FEEDBACK_HEIGHT = 610;

/**
 * @type {string}
 * @const
 */
const FEEDBACK_DEFAULT_WINDOW_ID = 'default_window';

// To generate a hashed extension ID, use a sha-1 hash, all in lower case.
// Example:
//   echo -n 'abcdefghijklmnopqrstuvwxyzabcdef' | sha1sum | \
//       awk '{print toupper($1)}'
const whitelistedExtensionIds = [
  '12E618C3C6E97495AAECF2AC12DEB082353241C6',  // QuickOffice
  '3727DD3E564B6055387425027AD74C58784ACC15',  // QuickOffice
  '2FC374607C2DF285634B67C64A2E356C607091C3',  // QuickOffice
  '2843C1E82A9B6C6FB49308FDDF4E157B6B44BC2B',  // G+ Photos
  '5B5DA6D054D10DB917AF7D9EAE3C56044D1B0B03',  // G+ Photos
  '986913085E3E3C3AFDE9B7A943149C4D3F4C937B',  // Feedback Extension
  '7AE714FFD394E073F0294CFA134C9F91DB5FBAA4',  // Connectivity Diagnostics
  'C7DA3A55C2355F994D3FDDAD120B426A0DF63843',  // Connectivity Diagnostics
  '75E3CFFFC530582C583E4690EF97C70B9C8423B7',  // Connectivity Diagnostics
  '32A1BA997F8AB8DE29ED1BA94AAF00CF2A3FEFA7',  // Connectivity Diagnostics
  'A291B26E088FA6BA53FFD72F0916F06EBA7C585A',  // Chrome OS Recovery Tool
  'D7986543275120831B39EF28D1327552FC343960',  // Chrome OS Recovery Tool
  '8EBDF73405D0B84CEABB8C7513C9B9FA9F1DC2CE',  // GetHelp app.
  '97B23E01B2AA064E8332EE43A7A85C628AADC3F2',  // Chrome Remote Desktop Dev
  '9E527CDA9D7C50844E8A5DB964A54A640AE48F98',  // Chrome Remote Desktop Stable
  'DF52618D0B040D8A054D8348D2E84DDEEE5974E7',  // Chrome Remote Desktop QA
  '269D721F163E587BC53C6F83553BF9CE2BB143CD',  // Chrome Remote Desktop QA
                                               // backup
  'C449A798C495E6CF7D6AF10162113D564E67AD12',  // Chrome Remote Desktop Apps V2
  '981974CD1832B87BE6B21BE78F7249BB501E0DE6',  // Play Movies Dev
  '32FD7A816E47392C92D447707A89EB07EEDE6FF7',  // Play Movies Nightly
  '3F3CEC4B9B2B5DC2F820CE917AABDF97DB2F5B49',  // Play Movies Beta
  'F92FAC70AB68E1778BF62D9194C25979596AA0E6',  // Play Movies Stable
  '0F585FB1D0FDFBEBCE1FEB5E9DFFB6DA476B8C9B',  // Hangouts Extension
  '2D22CDB6583FD0A13758AEBE8B15E45208B4E9A7',  // Hangouts Extension
  '49DA0B9CCEEA299186C6E7226FD66922D57543DC',  // Hangouts Extension
  'E7E2461CE072DF036CF9592740196159E2D7C089',  // Hangouts Extension
  'A74A4D44C7CFCD8844830E6140C8D763E12DD8F3',  // Hangouts Extension
  '312745D9BF916161191143F6490085EEA0434997',  // Hangouts Extension
  '53041A2FA309EECED01FFC751E7399186E860B2C',  // Hangouts Extension
  '0F42756099D914A026DADFA182871C015735DD95',  // Hangouts Extension
  '1B7734733E207CCE5C33BFAA544CA89634BF881F',  // GLS nightly
  'E2ACA3D943A3C96310523BCDFD8C3AF68387E6B7',  // GLS stable
  'BA007D8D52CC0E2632EFCA03ACD003B0F613FD71',  // http://crbug.com/470411
  '5260FA31DE2007A837B7F7B0EB4A47CE477018C8',  // http://crbug.com/470411
  '4F4A25F31413D9B9F80E61D096DEB09082515267',  // http://crbug.com/470411
  'FBA0DE4D3EFB5485FC03760F01F821466907A743',  // http://crbug.com/470411
  'E216473E4D15C5FB14522D32C5F8DEAAB2CECDC6',  // http://crbug.com/470411
  '676A08383D875E51CE4C2308D875AE77199F1413',  // http://crbug.com/473845
  '869A23E11B308AF45A68CC386C36AADA4BE44A01',  // http://crbug.com/473845
  'E9CE07C7EDEFE70B9857B312E88F94EC49FCC30F',  // http://crbug.com/473845
  'A4577D8C2AF4CF26F40CBCA83FFA4251D6F6C8F8',  // http://crbug.com/478929
  'A8208CCC87F8261AFAEB6B85D5E8D47372DDEA6B',  // http://crbug.com/478929
  // TODO (ntang) Remove the following 2 hashes by 12/31/2017.
  'B620CF4203315F9F2E046EDED22C7571A935958D',  // http://crbug.com/510270
  'B206D8716769728278D2D300349C6CB7D7DE2EF9',  // http://crbug.com/510270
  'EFCF5358672FEE04789FD2EC3638A67ADEDB6C8C',  // http://crbug.com/514696
  'FAD85BC419FE00995D196312F53448265EFA86F1',  // http://crbug.com/516527
  'F33B037DEDA65F226B7409C2ADB0CF3F8565AB03',  // http://crbug.com/541769
  '969C788BCBC82FBBE04A17360CA165C23A419257',  // http://crbug.com/541769
  '3BC3740BFC58F06088B300274B4CFBEA20136342',  // http://crbug.com/541769
  '2B6C6A4A5940017146F3E58B7F90116206E84685',  // http://crbug.com/642141
  '96FF2FFA5C9173C76D47184B3E86D267B37781DE',  // http://crbug.com/642141
  'A3E3DE9E9F16B41D4A2FAD106BD6CA76B94A0C94',  // http://crbug.com/908458
  'C2ABD68C33A5B485971C9638B80D6A2E9CBA78C4',  // http://crbug.com/908458
  'B41E7F08E1179CC03CBD1F49E57CF353A40ADE07',  // http://crbug.com/908458
  'A948368FC53BE437A55FEB414106E207925482F5',  // ChromeOS Files App.
];

/**
 * Used to generate unique IDs for FeedbackRequest objects.
 * @type {number}
 */
let lastUsedId = 0;

/**
 * A FeedbackRequest object represents a unique feedback report, requested by an
 * instance of the feedback window. It contains the system information specific
 * to this report, the full feedbackInfo, and callbacks to send the report upon
 * request.
 */
class FeedbackRequest {
  constructor(feedbackInfo) {
    this.id_ = ++lastUsedId;
    this.feedbackInfo_ = feedbackInfo;
    this.onSystemInfoReadyCallback_ = null;
    this.isSystemInfoReady_ = false;
    this.reportIsBeingSent_ = false;
    this.isRequestCanceled_ = false;
    this.useSystemInfo_ = false;
  }

  /**
   * Called when the system information is sent from the C++ side.
   * @param {Object} sysInfo The received system information.
   */
  getSystemInformationCallback(sysInfo) {
    if (this.isRequestCanceled_) {
      // If the window had been closed before the system information was
      // received, we skip the rest of the operations and return immediately.
      return;
    }

    this.isSystemInfoReady_ = true;

    // Combine the newly received system information with whatever system
    // information we have in the feedback info (if any).
    if (this.feedbackInfo_.systemInformation) {
      this.feedbackInfo_.systemInformation =
          this.feedbackInfo_.systemInformation.concat(sysInfo);
    } else {
      this.feedbackInfo_.systemInformation = sysInfo;
    }

    if (this.onSystemInfoReadyCallback_ != null) {
      this.onSystemInfoReadyCallback_();
      this.onSystemInfoReadyCallback_ = null;
    }
  }

  /**
   * Retrieves the system information for this request object.
   * @param {function()} callback Invoked to notify the listener that the system
   * information has been received.
   */
  getSystemInformation(callback) {
    if (this.isSystemInfoReady_) {
      callback();
      return;
    }

    this.onSystemInfoReadyCallback_ = callback;
    // The C++ side must reply to the callback specific to this object.
    const boundCallback = this.getSystemInformationCallback.bind(this);
    chrome.feedbackPrivate.getSystemInformation(boundCallback);
  }

  /**
   * Sends the feedback report represented by the object, either now if system
   * information is ready, or later once it is.
   * @param {boolean} useSystemInfo True if the user would like the system
   * information to be sent with the report.
   */
  sendReport(useSystemInfo) {
    this.reportIsBeingSent_ = true;
    this.useSystemInfo_ = useSystemInfo;
    if (useSystemInfo && !this.isSystemInfoReady_) {
      this.onSystemInfoReadyCallback_ = this.sendReportNow;
      return;
    }
    this.sendReportNow();
  }

  /**
   * Sends the report immediately and removes this object once the report is
   * sent.
   */
  sendReportNow() {
    if (!this.useSystemInfo_) {
      // Clear the system information if the user doesn't want it to be sent.
      this.feedbackInfo_.systemInformation = null;
    }

    /** @const */ const ID = this.id_;
    /** @const */ const FLOW = this.feedbackInfo_.flow;
    chrome.feedbackPrivate.sendFeedback(
        this.feedbackInfo_, function(result, landingPageType) {
          if (result == chrome.feedbackPrivate.Status.SUCCESS) {
            console.log('Feedback: Report sent for request with ID ' + ID);
            if (FLOW != chrome.feedbackPrivate.FeedbackFlow.LOGIN &&
                landingPageType !=
                    chrome.feedbackPrivate.LandingPageType.NO_LANDING_PAGE) {
              const landingPage = landingPageType ==
                      chrome.feedbackPrivate.LandingPageType.NORMAL ?
                  FEEDBACK_LANDING_PAGE :
                  FEEDBACK_LANDING_PAGE_TECHSTOP;
              window.open(landingPage, '_blank');
            }
          } else {
            console.log(
                'Feedback: Report for request with ID ' + ID +
                ' will be sent later.');
          }
          if (FLOW == chrome.feedbackPrivate.FeedbackFlow.LOGIN) {
            chrome.feedbackPrivate.loginFeedbackComplete();
          }
        });
  }

  /**
   * Handles the event when the feedback UI window corresponding to this
   * FeedbackRequest instance is closed.
   */
  onWindowClosed() {
    if (!this.reportIsBeingSent_) {
      this.isRequestCanceled_ = true;
      if (this.feedbackInfo_.flow ==
          chrome.feedbackPrivate.FeedbackFlow.LOGIN) {
        chrome.feedbackPrivate.loginFeedbackComplete();
      }
    }
  }
}

/**
 * Function to determine whether or not a given extension id is whitelisted to
 * invoke the feedback UI. If the extension is whitelisted, the callback to
 * start the Feedback UI will be called.
 * @param {string} id the id of the sender extension.
 * @param {Function} startFeedbackCallback The callback function that will
 *     will start the feedback UI.
 * @param {Object} feedbackInfo The feedback info object to pass to the
 *     start feedback UI callback.
 */
function senderWhitelisted(id, startFeedbackCallback, feedbackInfo) {
  crypto.subtle.digest('SHA-1', new TextEncoder().encode(id))
      .then(function(hashBuffer) {
        let hashString = '';
        const hashView = new Uint8Array(hashBuffer);
        for (let i = 0; i < hashView.length; ++i) {
          const n = hashView[i];
          hashString += n < 0x10 ? '0' : '';
          hashString += n.toString(16);
        }
        if (whitelistedExtensionIds.indexOf(hashString.toUpperCase()) != -1) {
          startFeedbackCallback(feedbackInfo);
        }
      });
}

/**
 * Callback which gets notified once our feedback UI has loaded and is ready to
 * receive its initial feedback info object.
 * @param {Object} request The message request object.
 * @param {Object} sender The sender of the message.
 * @param {function(Object)} sendResponse Callback for sending a response.
 */
function feedbackReadyHandler(request, sender, sendResponse) {
  if (request.ready) {
    chrome.runtime.sendMessage({sentFromEventPage: true});
  }
}

/**
 * Callback which gets notified if another extension is requesting feedback.
 * @param {Object} request The message request object.
 * @param {Object} sender The sender of the message.
 * @param {function(Object)} sendResponse Callback for sending a response.
 */
function requestFeedbackHandler(request, sender, sendResponse) {
  if (request.requestFeedback) {
    senderWhitelisted(sender.id, startFeedbackUI, request.feedbackInfo);
  }
}

/**
 * Callback which starts up the feedback UI.
 * @param {Object} feedbackInfo Object containing any initial feedback info.
 */
function startFeedbackUI(feedbackInfo) {
  const win = chrome.app.window.get(FEEDBACK_DEFAULT_WINDOW_ID);
  if (win) {
    win.show();
    return;
  }
  chrome.app.window.create(
      'html/default.html', {
        frame: feedbackInfo.useSystemWindowFrame ? 'chrome' : 'none',
        id: FEEDBACK_DEFAULT_WINDOW_ID,
        innerBounds: {
          minWidth: FEEDBACK_WIDTH,
          minHeight: FEEDBACK_HEIGHT,
        },
        hidden: true,
        resizable: false
      },
      function(appWindow) {
        const request = new FeedbackRequest(feedbackInfo);

        // The feedbackInfo member of the new window should refer to the one in
        // its corresponding FeedbackRequest object to avoid copying and
        // duplicatations.
        appWindow.contentWindow.feedbackInfo = request.feedbackInfo_;

        // Define some functions for the new window so that it can call back
        // into here.

        // Define a function for the new window to get the system information.
        appWindow.contentWindow.getSystemInformation = function(callback) {
          request.getSystemInformation(callback);
        };

        // Define a function to request sending the feedback report.
        appWindow.contentWindow.sendFeedbackReport = function(useSystemInfo) {
          request.sendReport(useSystemInfo);
        };

        // Observe when the window is closed.
        appWindow.onClosed.addListener(function() {
          request.onWindowClosed();
        });
      });
}

chrome.runtime.onMessage.addListener(feedbackReadyHandler);
chrome.runtime.onMessageExternal.addListener(requestFeedbackHandler);
chrome.feedbackPrivate.onFeedbackRequested.addListener(startFeedbackUI);
