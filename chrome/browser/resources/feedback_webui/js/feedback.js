// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
import {sendWithPromise} from 'chrome://resources/js/cr.m.js';
import {$} from 'chrome://resources/js/util.m.js';

import {FEEDBACK_LANDING_PAGE, FEEDBACK_LANDING_PAGE_TECHSTOP, FEEDBACK_LEGAL_HELP_URL, FEEDBACK_PRIVACY_POLICY_URL, FEEDBACK_TERM_OF_SERVICE_URL, openUrlInAppWindow} from './feedback_util.js';
import {takeScreenshot} from './take_screenshot.js';


/** @type {string} */
const dialogArgs = chrome.getVariableValue('dialogArguments');

/**
 * The object will be manipulated by feedbackHelper
 *
 * @type {chrome.feedbackPrivate.FeedbackInfo}
 */
let feedbackInfo = {
  assistantDebugInfoAllowed: false,
  attachedFile: undefined,
  attachedFileBlobUuid: undefined,
  categoryTag: undefined,
  description: '...',
  descriptionPlaceholder: undefined,
  email: undefined,
  flow: chrome.feedbackPrivate.FeedbackFlow.REGULAR,
  fromAssistant: false,
  includeBluetoothLogs: false,
  pageUrl: undefined,
  sendHistograms: undefined,
  systemInformation: [],
  useSystemWindowFrame: false,
};


class FeedbackHelper {
  constructor() {
    /**
     * @type {boolean}
     */
    this.systemInformationLoaded = false;
  }

  getSystemInformation() {
    return new Promise(
        resolve => chrome.feedbackPrivate.getSystemInformation(resolve));
  }

  getFullSystemInformation() {
    return new Promise(resolve => {
      if (this.systemInformationLoaded) {
        resolve(feedbackInfo.systemInformation);
      } else {
        this.getSystemInformation().then(function(sysInfo) {
          if (feedbackInfo.systemInformation) {
            feedbackInfo.systemInformation =
                feedbackInfo.systemInformation.concat(sysInfo);
          } else {
            feedbackInfo.systemInformation = sysInfo;
          }
          this.systemInformationLoaded = true;
          resolve(feedbackInfo.systemInformation);
        });
      }
    });
  }

  getUserEmail() {
    return new Promise(resolve => chrome.feedbackPrivate.getUserEmail(resolve));
  }

  /**
   * @param {boolean} useSystemInfo
   */
  sendFeedbackReport(useSystemInfo) {
    const ID = Math.round(Date.now() / 1000);
    const FLOW = feedbackInfo.flow;
    if (!useSystemInfo) {
      this.systemInformationLoaded = false;
      feedbackInfo.systemInformation = [];
    }

    chrome.feedbackPrivate.sendFeedback(
        feedbackInfo, function(result, landingPageType) {
          if (result == chrome.feedbackPrivate.Status.SUCCESS) {
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
            console.warn(
                'Feedback: Report for request with ID ' + ID +
                ' will be sent later.');
          }
          if (FLOW == chrome.feedbackPrivate.FeedbackFlow.LOGIN) {
            chrome.feedbackPrivate.loginFeedbackComplete();
          }
        });
  }

  // Send a message to show the WebDialog
  showDialog() {
    chrome.send('showDialog');
  }

  // Send a message to close the WebDialog
  closeDialog() {
    chrome.send('dialogClose');
  }
}

/**
 * @type {FeedbackHelper}
 * @const
 */
const feedbackHelper = new FeedbackHelper();

/** @type {number}
 * @const
 */
const MAX_ATTACH_FILE_SIZE = 3 * 1024 * 1024;

/**
 * @type {number}
 * @const
 */
const FEEDBACK_MIN_WIDTH = 500;

/**
 * @type {number}
 * @const
 */
const FEEDBACK_MIN_HEIGHT = 610;

/**
 * @type {number}
 * @const
 */
const FEEDBACK_MIN_HEIGHT_LOGIN = 482;

/** @type {number}
 * @const
 */
const MAX_SCREENSHOT_WIDTH = 100;

/** @type {string}
 * @const
 */
const SYSINFO_WINDOW_ID = 'sysinfo_window';

/**
 * @type {Blob}
 */
let attachedFileBlob = null;
const lastReader = null;

/**
 * Regular expression to check for all variants of blu[e]toot[h] with or without
 * space between the words; for BT when used as an individual word, or as two
 * individual characters, and for BLE when used as an individual word. Case
 * insensitive matching.
 * @type {RegExp}
 */
const btRegEx = new RegExp('blu[e]?[ ]?toot[h]?|\\bb[ ]?t\\b|\\bble\\b', 'i');

/**
 * Regular expression to check for all strings indicating that a user can't
 * connect to a HID or Audio device. This is also a likely indication of a
 * Bluetooth related issue.
 * Sample strings this will match:
 * "I can't connect the speaker!",
 * "The keyboard has connection problem."
 * @type {RegExp}
 */
const cantConnectRegEx = new RegExp(
    '((headphone|keyboard|mouse|speaker)((?!(connect|pair)).*)(connect|pair))' +
        '|((connect|pair).*(headphone|keyboard|mouse|speaker))',
    'i');

/**
 * Regular expression to check for "tether" or "tethering". Case insensitive
 * matching.
 * @type {RegExp}
 */
const tetherRegEx = new RegExp('tether(ing)?', 'i');

/**
 * Regular expression to check for "Smart (Un)lock" or "Easy (Un)lock" with or
 * without space between the words. Case insensitive matching.
 * @type {RegExp}
 */
const smartLockRegEx = new RegExp('(smart|easy)[ ]?(un)?lock', 'i');

/**
 * Regular expression to check for keywords related to Nearby Share like
 * "nearby (share)" or "phone (hub)".
 * Case insensitive matching.
 * @type {RegExp}
 */
const nearbyShareRegEx = new RegExp('nearby|phone', 'i');

/**
 * Reads the selected file when the user selects a file.
 * @param {Event} fileSelectedEvent The onChanged event for the file input box.
 */
function onFileSelected(fileSelectedEvent) {
  const file = fileSelectedEvent.target.files[0];
  if (!file) {
    // User canceled file selection.
    attachedFileBlob = null;
    return;
  }

  if (file.size > MAX_ATTACH_FILE_SIZE) {
    $('attach-error').hidden = false;

    // Clear our selected file.
    $('attach-file').value = '';
    attachedFileBlob = null;
    return;
  }

  attachedFileBlob = file.slice();
}

/**
 * Called when user opens the file dialog. Hide $('attach-error') before file
 * dialog is open to prevent a11y bug https://crbug.com/1020047
 */
function onOpenFileDialog() {
  $('attach-error').hidden = true;
}

/**
 * Clears the file that was attached to the report with the initial request.
 * Instead we will now show the attach file button in case the user wants to
 * attach another file.
 */
function clearAttachedFile() {
  $('custom-file-container').hidden = true;
  attachedFileBlob = null;
  feedbackInfo.attachedFile = undefined;
  $('attach-file').hidden = false;
}

/**
 * Sets up the event handlers for the given |anchorElement|.
 * @param {HTMLElement} anchorElement The <a> html element.
 * @param {string} url The destination URL for the link.
 * @param {boolean} useAppWindow true if the URL should be opened inside a new
 *                  App Window, false if it should be opened in a new tab.
 */
function setupLinkHandlers(anchorElement, url, useAppWindow) {
  anchorElement.onclick = function(e) {
    e.preventDefault();
    if (useAppWindow) {
      openUrlInAppWindow(url);
    } else {
      window.open(url, '_blank');
    }
  };

  anchorElement.onauxclick = function(e) {
    e.preventDefault();
  };
}

/**
 * Opens a new window with chrome://slow_trace, downloading performance data.
 */
function openSlowTraceWindow() {
  window.open('chrome://slow_trace/tracing.zip#' + feedbackInfo.traceId);
}

/**
 * Checks if any keywords related to bluetooth have been typed. If they are,
 * we show the bluetooth logs option, otherwise hide it.
 * @param {Event} inputEvent The input event for the description textarea.
 */
function checkForBluetoothKeywords(inputEvent) {
  const isRelatedToBluetooth = btRegEx.test(inputEvent.target.value) ||
      cantConnectRegEx.test(inputEvent.target.value) ||
      tetherRegEx.test(inputEvent.target.value) ||
      smartLockRegEx.test(inputEvent.target.value) ||
      nearbyShareRegEx.test(inputEvent.target.value);
  $('bluetooth-checkbox-container').hidden = !isRelatedToBluetooth;
}

/**
 * Updates the description-text box based on whether it was valid.
 * If invalid, indicate an error to the user. If valid, remove indication of the
 * error.
 */
function updateDescription(wasValid) {
  // Set visibility of the alert text for users who don't use a screen
  // reader.
  $('description-empty-error').hidden = wasValid;

  // Change the textarea's aria-labelled by to ensure the screen reader does
  // (or doesn't) read the error, as appropriate.
  // If it does read the error, it should do so _before_ it reads the normal
  // description.
  const description = $('description-text');
  description.setAttribute(
      'aria-labelledby',
      (wasValid ? '' : 'description-empty-error ') + 'free-form-text');
  // Indicate whether input is valid.
  description.setAttribute('aria-invalid', !wasValid);
  if (!wasValid) {
    // Return focus to field so user can correct error.
    description.focus();
  }

  // We may have added or removed a line of text, so make sure the app window
  // is the right size.
  resizeAppWindow();
}

/**
 * Sends the report; after the report is sent, we need to be redirected to
 * the landing page, but we shouldn't be able to navigate back, hence
 * we open the landing page in a new tab and sendReport closes this tab.
 * @return {boolean} True if the report was sent.
 */
function sendReport() {
  if ($('description-text').value.length == 0) {
    updateDescription(false);
    return false;
  }
  // This isn't strictly necessary, since if we get past this point we'll
  // succeed, but for future-compatibility (and in case we later add more
  // failure cases after this), re-hide the alert and reset the aria label.
  updateDescription(true);

  // Prevent double clicking from sending additional reports.
  $('send-report-button').disabled = true;
  if (!feedbackInfo.attachedFile && attachedFileBlob) {
    feedbackInfo.attachedFile = {
      name: $('attach-file').value,
      data: attachedFileBlob,
    };
  }

  feedbackInfo.description = $('description-text').value;
  feedbackInfo.pageUrl = $('page-url-text').value;
  feedbackInfo.email = $('user-email-drop-down').value;

  let useSystemInfo = false;
  let useHistograms = false;
  if ($('sys-info-checkbox') != null && $('sys-info-checkbox').checked) {
    // Send histograms along with system info.
    useSystemInfo = useHistograms = true;
  }

  // <if expr="chromeos">
  if ($('assistant-info-checkbox') != null &&
      $('assistant-info-checkbox').checked &&
      !$('assistant-checkbox-container').hidden) {
    // User consent to link Assistant debug info on Assistant server.
    feedbackInfo.assistantDebugInfoAllowed = true;
  }
  // </if>

  // <if expr="chromeos">
  if ($('bluetooth-logs-checkbox') != null &&
      $('bluetooth-logs-checkbox').checked &&
      !$('bluetooth-checkbox-container').hidden) {
    feedbackInfo.sendBluetoothLogs = true;
    feedbackInfo.categoryTag = 'BluetoothReportWithLogs';
  }
  if ($('performance-info-checkbox') == null ||
      !($('performance-info-checkbox').checked)) {
    feedbackInfo.traceId = undefined;
  }
  // </if>

  feedbackInfo.sendHistograms = useHistograms;

  if ($('screenshot-checkbox').checked) {
    // The user is okay with sending the screenshot and tab titles.
    feedbackInfo.sendTabTitles = true;
  } else {
    // The user doesn't want to send the screenshot, so clear it.
    feedbackInfo.screenshot = null;
  }

  let productId = parseInt('' + feedbackInfo.productId, 10);
  if (isNaN(productId)) {
    // For apps that still use a string value as the |productId|, we must clear
    // that value since the API uses an integer value, and a conflict in data
    // types will cause the report to fail to be sent.
    productId = undefined;
  }
  feedbackInfo.productId = productId;

  // Request sending the report, show the landing page (if allowed)
  feedbackHelper.sendFeedbackReport(useSystemInfo);
  scheduleWindowClose();
  return true;
}

/**
 * Click listener for the cancel button.
 * @param {Event} e The click event being handled.
 */
function cancel(e) {
  e.preventDefault();
  scheduleWindowClose();
  if (feedbackInfo.flow == chrome.feedbackPrivate.FeedbackFlow.LOGIN) {
    chrome.feedbackPrivate.loginFeedbackComplete();
  }
}

// <if expr="chromeos">
/**
 * Update the page when performance feedback state is changed.
 */
function performanceFeedbackChanged() {
  if ($('performance-info-checkbox').checked) {
    $('attach-file').disabled = true;
    $('attach-file').checked = false;

    $('screenshot-checkbox').disabled = true;
    $('screenshot-checkbox').checked = false;
  } else {
    $('attach-file').disabled = false;
    $('screenshot-checkbox').disabled = false;
  }
}
// </if>

function resizeAppWindow() {
  // TODO(crbug.com/1167223): The UI is now controlled by a WebDialog delegate
  // which is set to not resizable for now. If needed, a message handler can
  // be added to respond to resize request.
}

/**
 * Close the window after 100ms delay.
 */
function scheduleWindowClose() {
  setTimeout(function() {
    feedbackHelper.closeDialog();
  }, 100);
}

/**
 * Initializes our page.
 * Flow:
 * .) DOMContent Loaded        -> . Request feedbackInfo object
 *                                . Setup page event handlers
 * .) Feedback Object Received -> . take screenshot
 *                                . request email
 *                                . request System info
 *                                . request i18n strings
 * .) Screenshot taken         -> . Show Feedback window.
 */
function initialize() {
  // apply received feedback info object.
  const applyData = function(feedbackInfo) {
    if (feedbackInfo.includeBluetoothLogs) {
      assert(
          feedbackInfo.flow ==
          chrome.feedbackPrivate.FeedbackFlow.GOOGLE_INTERNAL);
      $('description-text')
          .addEventListener('input', checkForBluetoothKeywords);
    }

    if ($('assistant-checkbox-container') != null &&
        feedbackInfo.flow ==
            chrome.feedbackPrivate.FeedbackFlow.GOOGLE_INTERNAL &&
        feedbackInfo.fromAssistant) {
      $('assistant-checkbox-container').hidden = false;
    }

    $('description-text').textContent = feedbackInfo.description;
    if (feedbackInfo.descriptionPlaceholder) {
      $('description-text').placeholder = feedbackInfo.descriptionPlaceholder;
    }
    if (feedbackInfo.pageUrl) {
      $('page-url-text').value = feedbackInfo.pageUrl;
    }

    takeScreenshot(function(screenshotCanvas) {
      // We've taken our screenshot, show the feedback page without any
      // further delay.
      window.requestAnimationFrame(function() {
        resizeAppWindow();
      });

      feedbackHelper.showDialog();

      // Allow feedback to be sent even if the screenshot failed.
      if (!screenshotCanvas) {
        $('screenshot-checkbox').disabled = true;
        $('screenshot-checkbox').checked = false;
        return;
      }

      screenshotCanvas.toBlob(function(blob) {
        $('screenshot-image').src = URL.createObjectURL(blob);
        // Only set the alt text when the src url is available, otherwise we'd
        // get a broken image picture instead. crbug.com/773985.
        $('screenshot-image').alt = 'screenshot';
        $('screenshot-image')
            .classList.toggle(
                'wide-screen',
                $('screenshot-image').width > MAX_SCREENSHOT_WIDTH);
        feedbackInfo.screenshot = blob;
      });
    });

    feedbackHelper.getUserEmail().then(function(email) {
      // Never add an empty option.
      if (!email) {
        return;
      }
      const optionElement = document.createElement('option');
      optionElement.value = email;
      optionElement.text = email;
      optionElement.selected = true;
      // Make sure the "Report anonymously" option comes last.
      $('user-email-drop-down')
          .insertBefore(optionElement, $('anonymous-user-option'));

      // Now we can unhide the user email section:
      $('user-email').hidden = false;
    });

    // An extension called us with an attached file.
    if (feedbackInfo.attachedFile) {
      $('attached-filename-text').textContent = feedbackInfo.attachedFile.name;
      attachedFileBlob = feedbackInfo.attachedFile.data;
      $('custom-file-container').hidden = false;
      $('attach-file').hidden = true;
    }

    // No URL, file attachment for login screen feedback.
    if (feedbackInfo.flow == chrome.feedbackPrivate.FeedbackFlow.LOGIN) {
      $('page-url').hidden = true;
      $('attach-file-container').hidden = true;
      $('attach-file-note').hidden = true;
    }

    // <if expr="chromeos">
    if (feedbackInfo.traceId && ($('performance-info-area'))) {
      $('performance-info-area').hidden = false;
      $('performance-info-checkbox').checked = true;
      performanceFeedbackChanged();
      $('performance-info-link').onclick = openSlowTraceWindow;
    }
    // </if>

    const sysInfoUrlElement = $('sys-info-url');
    if (sysInfoUrlElement) {
      // Opens a new window showing the full anonymized system+app
      // information.
      sysInfoUrlElement.onclick = function(e) {
        e.preventDefault();
        const params = `status=no,location=no,toolbar=no,menubar=no,
              width=640,height=400,left=200,top=200`;

        const sysWin =
            window.open('/html/sys_info.html', SYSINFO_WINDOW_ID, params);

        if (sysWin) {
          sysWin.window.getFullSystemInfo = feedbackHelper.getSystemInformation;
        }
      };

      sysInfoUrlElement.onauxclick = function(e) {
        e.preventDefault();
      };
    }

    const histogramUrlElement = $('histograms-url');
    if (histogramUrlElement) {
      // Opens a new window showing the histogram metrics.
      setupLinkHandlers(
          histogramUrlElement, 'chrome://histograms', true /* useAppWindow */);
    }

    // The following URLs don't open on login screen, so hide them.
    // TODO(crbug.com/1116383): Find a solution to display them properly.
    if (feedbackInfo.flow != chrome.feedbackPrivate.FeedbackFlow.LOGIN) {
      const legalHelpPageUrlElement = $('legal-help-page-url');
      if (legalHelpPageUrlElement) {
        setupLinkHandlers(
            legalHelpPageUrlElement, FEEDBACK_LEGAL_HELP_URL,
            false /* useAppWindow */);
      }

      const privacyPolicyUrlElement = $('privacy-policy-url');
      if (privacyPolicyUrlElement) {
        setupLinkHandlers(
            privacyPolicyUrlElement, FEEDBACK_PRIVACY_POLICY_URL,
            false /* useAppWindow */);
      }

      const termsOfServiceUrlElement = $('terms-of-service-url');
      if (termsOfServiceUrlElement) {
        setupLinkHandlers(
            termsOfServiceUrlElement, FEEDBACK_TERM_OF_SERVICE_URL,
            false /* useAppWindow */);
      }

      const bluetoothLogsInfoLinkElement = $('bluetooth-logs-info-link');
      if (bluetoothLogsInfoLinkElement) {
        bluetoothLogsInfoLinkElement.onclick = function(e) {
          e.preventDefault();

          const params = `status=no,location=no,toolbar=no,menubar=no,
              width=400,height=120,left=200,top=200,resizable=no,`;

          const blueToothWin = window.open(
              '/html/bluetooth_logs_info.html', 'bluetooth_logs_window',
              params);

          bluetoothLogsInfoLinkElement.onauxclick = function(e) {
            e.preventDefault();
          };
        };
      }

      const assistantLogsInfoLinkElement = $('assistant-logs-info-link');
      if (assistantLogsInfoLinkElement) {
        assistantLogsInfoLinkElement.onclick = function(e) {
          e.preventDefault();

          const params = `status=no,location=no,toolbar=no,menubar=no,
              width=400,height=120,left=200,top=200,resizable=no,`;

          const blueToothWin = window.open(
              '/html/assistant_logs_info.html', 'assistant_logs_window',
              params);

          assistantLogsInfoLinkElement.onauxclick = function(e) {
            e.preventDefault();
          };
        };
      }
    }

    // Make sure our focus starts on the description field.
    $('description-text').focus();
  };

  window.addEventListener('DOMContentLoaded', function() {
    if (dialogArgs) {
      feedbackInfo = /** @type {chrome.feedbackPrivate.FeedbackInfo} */ (
          JSON.parse(dialogArgs));
    }
    applyData(feedbackInfo);

    window.feedbackInfo = feedbackInfo;
    window.feedbackHelper = feedbackHelper;

    // Setup our event handlers.
    $('attach-file').addEventListener('change', onFileSelected);
    $('attach-file').addEventListener('click', onOpenFileDialog);
    $('send-report-button').onclick = sendReport;
    $('cancel-button').onclick = cancel;
    $('remove-attached-file').onclick = clearAttachedFile;
    // <if expr="chromeos">
    $('performance-info-checkbox')
        .addEventListener('change', performanceFeedbackChanged);
    // </if>
  });
}

initialize();
