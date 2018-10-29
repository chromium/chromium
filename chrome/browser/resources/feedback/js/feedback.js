// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @type {string}
 * @const
 */
var SRT_DOWNLOAD_PAGE = 'https://www.google.com/chrome/cleanup-tool/';

/** @type {number}
 * @const
 */
var MAX_ATTACH_FILE_SIZE = 3 * 1024 * 1024;

/**
 * @type {number}
 * @const
 */
var FEEDBACK_MIN_WIDTH = 500;

/**
 * @type {number}
 * @const
 */
var FEEDBACK_MIN_HEIGHT = 585;

/**
 * @type {number}
 * @const
 */
var FEEDBACK_MIN_HEIGHT_LOGIN = 482;

/** @type {number}
 * @const
 */
var CONTENT_MARGIN_HEIGHT = 40;

/** @type {number}
 * @const
 */
var MAX_SCREENSHOT_WIDTH = 100;

/** @type {string}
 * @const
 */
var SYSINFO_WINDOW_ID = 'sysinfo_window';

/** @type {string}
 * @const
 */
var STATS_WINDOW_ID = 'stats_window';

/**
 * SRT Prompt Result defined in feedback_private.idl.
 * @enum {string}
 */
var SrtPromptResult = {
  ACCEPTED: 'accepted',  // User accepted prompt.
  DECLINED: 'declined',  // User declined prompt.
  CLOSED: 'closed',      // User closed window without responding to prompt.
};

var attachedFileBlob = null;
var lastReader = null;

/**
 * Determines whether the system information associated with this instance of
 * the feedback window has been received.
 * @type {boolean}
 */
var isSystemInfoReady = false;

/**
 * Indicates whether the SRT Prompt is currently being displayed.
 * @type {boolean}
 */
var isShowingSrtPrompt = false;

/**
 * Regular expression to check for all variants of bluetooth, blutooth, with or
 * without space between the words and for BT when used as an individual word,
 * or as two individual characters. Case insensitive matching.
 * @type {RegExp}
 */
const btRegEx = new RegExp('[b]lu[e]?[ ]?tooth|\b[b][ ]?[t]\b', 'i');

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
 * The callback used by the sys_info_page to receive the event that the system
 * information is ready.
 * @type {function(sysInfo)}
 */
var sysInfoPageOnSysInfoReadyCallback = null;

/**
 * Reads the selected file when the user selects a file.
 * @param {Event} fileSelectedEvent The onChanged event for the file input box.
 */
function onFileSelected(fileSelectedEvent) {
  $('attach-error').hidden = true;
  var file = fileSelectedEvent.target.files[0];
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
 * Clears the file that was attached to the report with the initial request.
 * Instead we will now show the attach file button in case the user wants to
 * attach another file.
 */
function clearAttachedFile() {
  $('custom-file-container').hidden = true;
  attachedFileBlob = null;
  feedbackInfo.attachedFile = null;
  $('attach-file').hidden = false;
}

/**
 * Creates a closure that creates or shows a window with the given url.
 * @param {string} windowId A string with the ID of the window we are opening.
 * @param {string} url The destination URL of the new window.
 * @return {function()} A function to be called to open the window.
 */
function windowOpener(windowId, url) {
  return function(e) {
    e.preventDefault();
    chrome.app.window.create(url, {id: windowId});
  };
}

/**
 * Sets up the event handlers for the given |anchorElement|.
 * @param {HTMLElement} anchorElement The <a> html element.
 * @param {string} url The destination URL for the link.
 */
function setupLinkHandlers(anchorElement, url) {
  anchorElement.onclick = function(e) {
    e.preventDefault();
    window.open(url, '_blank');
  };

  anchorElement.onauxclick = function(e) {
    e.preventDefault();
  };
}

/**
 * Opens a new window with chrome://slow_trace, downloading performance data.
 */
function openSlowTraceWindow() {
  chrome.app.window.create(
      'chrome://slow_trace/tracing.zip#' + feedbackInfo.traceId);
}

/**
 * Checks if any keywords related to bluetooth have been typed. If they are,
 * we show the bluetooth logs option, otherwise hide it.
 * @param {Event} inputEvent The input event for the description textarea.
 */
function checkForBluetoothKeywords(inputEvent) {
  var isRelatedToBluetooth = btRegEx.test(inputEvent.target.value) ||
      cantConnectRegEx.test(inputEvent.target.value) ||
      tetherRegEx.test(inputEvent.target.value) ||
      smartLockRegEx.test(inputEvent.target.value);
  $('bluetooth-checkbox-container').hidden = !isRelatedToBluetooth;
}

/**
 * Sends the report; after the report is sent, we need to be redirected to
 * the landing page, but we shouldn't be able to navigate back, hence
 * we open the landing page in a new tab and sendReport closes this tab.
 * @return {boolean} True if the report was sent.
 */
function sendReport() {
  if ($('description-text').value.length == 0) {
    var description = $('description-text');
    description.placeholder = loadTimeData.getString('no-description');
    description.focus();
    return false;
  }

  // Prevent double clicking from sending additional reports.
  $('send-report-button').disabled = true;
  console.log('Feedback: Sending report');
  if (!feedbackInfo.attachedFile && attachedFileBlob) {
    feedbackInfo.attachedFile = {
      name: $('attach-file').value,
      data: attachedFileBlob
    };
  }

  feedbackInfo.description = $('description-text').value;
  feedbackInfo.pageUrl = $('page-url-text').value;
  feedbackInfo.email = $('user-email-drop-down').value;

  var useSystemInfo = false;
  var useHistograms = false;
  if ($('sys-info-checkbox') != null && $('sys-info-checkbox').checked) {
    // Send histograms along with system info.
    useSystemInfo = useHistograms = true;
  }
  // <if expr="chromeos">
  if ($('bluetooth-logs-checkbox') != null &&
      $('bluetooth-logs-checkbox').checked &&
      !$('bluetooth-checkbox-container').hidden) {
    feedbackInfo.sendBluetoothLogs = true;
    feedbackInfo.categoryTag = 'BluetoothReportWithLogs';
  }
  if ($('performance-info-checkbox') == null ||
      !($('performance-info-checkbox').checked)) {
    feedbackInfo.traceId = null;
  }
  // </if>

  feedbackInfo.sendHistograms = useHistograms;

  // If the user doesn't want to send the screenshot.
  if (!$('screenshot-checkbox').checked)
    feedbackInfo.screenshot = null;

  var productId = parseInt('' + feedbackInfo.productId);
  if (isNaN(productId)) {
    // For apps that still use a string value as the |productId|, we must clear
    // that value since the API uses an integer value, and a conflict in data
    // types will cause the report to fail to be sent.
    productId = null;
  }
  feedbackInfo.productId = productId;

  // Request sending the report, show the landing page (if allowed), and close
  // this window right away. The FeedbackRequest object that represents this
  // report will take care of sending the report in the background.
  sendFeedbackReport(useSystemInfo);
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
  // We pick the width from the titlebar, which has no margins.
  var width = $('title-bar').scrollWidth;
  if (width < FEEDBACK_MIN_WIDTH)
    width = FEEDBACK_MIN_WIDTH;

  // We get the height by adding the titlebar height and the content height +
  // margins. We can't get the margins for the content-pane here by using
  // style.margin - the variable seems to not exist.
  var height = $('title-bar').scrollHeight + $('content-pane').scrollHeight +
      CONTENT_MARGIN_HEIGHT;

  var minHeight = FEEDBACK_MIN_HEIGHT;
  if (feedbackInfo.flow == chrome.feedbackPrivate.FeedbackFlow.LOGIN)
    minHeight = FEEDBACK_MIN_HEIGHT_LOGIN;
  height = Math.max(height, minHeight);

  chrome.app.window.current().resizeTo(width, height);
}

/**
 * A callback to be invoked when the background page of this extension receives
 * the system information.
 */
function onSystemInformation() {
  isSystemInfoReady = true;
  // In case the sys_info_page needs to be notified by this event, do so.
  if (sysInfoPageOnSysInfoReadyCallback != null) {
    sysInfoPageOnSysInfoReadyCallback(feedbackInfo.systemInformation);
    sysInfoPageOnSysInfoReadyCallback = null;
  }
}

/**
 * Close the window after 100ms delay.
 */
function scheduleWindowClose() {
  setTimeout(function() {
    window.close();
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
  // Add listener to receive the feedback info object.
  chrome.runtime.onMessage.addListener(function(request, sender, sendResponse) {
    if (request.sentFromEventPage) {
      if (!feedbackInfo.flow)
        feedbackInfo.flow = chrome.feedbackPrivate.FeedbackFlow.REGULAR;

      if (feedbackInfo.flow ==
          chrome.feedbackPrivate.FeedbackFlow.SHOW_SRT_PROMPT) {
        isShowingSrtPrompt = true;
        $('content-pane').hidden = true;

        $('srt-decline-button').onclick = function() {
          isShowingSrtPrompt = false;
          chrome.feedbackPrivate.logSrtPromptResult(SrtPromptResult.DECLINED);
          $('srt-prompt').hidden = true;
          $('content-pane').hidden = false;
        };

        $('srt-accept-button').onclick = function() {
          chrome.feedbackPrivate.logSrtPromptResult(SrtPromptResult.ACCEPTED);
          window.open(SRT_DOWNLOAD_PAGE, '_blank');
          scheduleWindowClose();
        };

        $('close-button').addEventListener('click', function() {
          if (isShowingSrtPrompt) {
            chrome.feedbackPrivate.logSrtPromptResult(SrtPromptResult.CLOSED);
          }
        });
      } else if (
          feedbackInfo.flow ==
          chrome.feedbackPrivate.FeedbackFlow.GOOGLE_INTERNAL) {
        $('description-text')
            .addEventListener('input', checkForBluetoothKeywords);
        $('srt-prompt').hidden = true;
      } else {
        $('srt-prompt').hidden = true;
      }

      $('description-text').textContent = feedbackInfo.description;
      if (feedbackInfo.descriptionPlaceholder)
        $('description-text').placeholder = feedbackInfo.descriptionPlaceholder;
      if (feedbackInfo.pageUrl)
        $('page-url-text').value = feedbackInfo.pageUrl;

      takeScreenshot(function(screenshotCanvas) {
        // We've taken our screenshot, show the feedback page without any
        // further delay.
        window.webkitRequestAnimationFrame(function() {
          resizeAppWindow();
        });
        chrome.app.window.current().show();

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

      chrome.feedbackPrivate.getUserEmail(function(email) {
        // Never add an empty option.
        if (!email)
          return;
        var optionElement = document.createElement('option');
        optionElement.value = email;
        optionElement.text = email;
        optionElement.selected = true;
        // Make sure the "Report anonymously" option comes last.
        $('user-email-drop-down')
            .insertBefore(optionElement, $('anonymous-user-option'));

        // Now we can unhide the user email section:
        $('user-email').hidden = false;
      });

      // Initiate getting the system info.
      isSystemInfoReady = false;
      getSystemInformation(onSystemInformation);

      // An extension called us with an attached file.
      if (feedbackInfo.attachedFile) {
        $('attached-filename-text').textContent =
            feedbackInfo.attachedFile.name;
        attachedFileBlob = feedbackInfo.attachedFile.data;
        $('custom-file-container').hidden = false;
        $('attach-file').hidden = true;
      }

      // No URL and file attachment for login screen feedback.
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
      chrome.feedbackPrivate.getStrings(feedbackInfo.flow, function(strings) {
        loadTimeData.data = strings;
        i18nTemplate.process(document, loadTimeData);

        var sysInfoUrlElement = $('sys-info-url');
        if (sysInfoUrlElement) {
          // Opens a new window showing the full anonymized system+app
          // information.
          sysInfoUrlElement.onclick = function(e) {
            e.preventDefault();
            var win = chrome.app.window.get(SYSINFO_WINDOW_ID);
            if (win) {
              win.show();
              return;
            }
            chrome.app.window.create(
                '/html/sys_info.html', {
                  frame: 'chrome',
                  id: SYSINFO_WINDOW_ID,
                  width: 640,
                  height: 400,
                  hidden: false,
                  resizable: true
                },
                function(appWindow) {
                  // Define functions for the newly created window.

                  // Gets the full system information for the new window.
                  appWindow.contentWindow.getFullSystemInfo = function(
                      callback) {
                    if (isSystemInfoReady) {
                      callback(feedbackInfo.systemInformation);
                      return;
                    }

                    sysInfoPageOnSysInfoReadyCallback = callback;
                  };

                  // Returns the loadTimeData for the new window.
                  appWindow.contentWindow.getLoadTimeData = function() {
                    return loadTimeData;
                  };
                });
          };

          sysInfoUrlElement.onauxclick = function(e) {
            e.preventDefault();
          };
        }

        var histogramUrlElement = $('histograms-url');
        if (histogramUrlElement) {
          // Opens a new window showing the histogram metrics.
          histogramUrlElement.onclick =
              windowOpener(STATS_WINDOW_ID, 'chrome://histograms');

          histogramUrlElement.onauxclick = function(e) {
            e.preventDefault();
          };
        }

        var legalHelpPageUrlElement = $('legal-help-page-url');
        if (legalHelpPageUrlElement)
          setupLinkHandlers(legalHelpPageUrlElement, FEEDBACK_LEGAL_HELP_URL);

        var privacyPolicyUrlElement = $('privacy-policy-url');
        if (privacyPolicyUrlElement) {
          setupLinkHandlers(
              privacyPolicyUrlElement, FEEDBACK_PRIVACY_POLICY_URL);
        }

        var termsOfServiceUrlElement = $('terms-of-service-url');
        if (termsOfServiceUrlElement) {
          setupLinkHandlers(
              termsOfServiceUrlElement, FEEDBACK_TERM_OF_SERVICE_URL);
        }

        var bluetoothLogsInfoLinkElement = $('bluetooth-logs-info-link');
        if (bluetoothLogsInfoLinkElement) {
          bluetoothLogsInfoLinkElement.onclick = function(e) {
            e.preventDefault();

            chrome.app.window.create(
                '/html/bluetooth_logs_info.html',
                {width: 400, height: 120, resizable: false},
                function(appWindow) {
                  appWindow.contentWindow.onload = function() {
                    i18nTemplate.process(
                        appWindow.contentWindow.document, loadTimeData);
                  };
                });

            bluetoothLogsInfoLinkElement.onauxclick = function(e) {
              e.preventDefault();
            };
          };
        }

        // Make sure our focus starts on the description field.
        $('description-text').focus();
      });
    }
  });

  window.addEventListener('DOMContentLoaded', function() {
    // Ready to receive the feedback object.
    chrome.runtime.sendMessage({ready: true});

    // Setup our event handlers.
    $('attach-file').addEventListener('change', onFileSelected);
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
