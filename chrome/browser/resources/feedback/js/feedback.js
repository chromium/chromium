// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
const FEEDBACK_MIN_HEIGHT = 585;

/**
 * @type {number}
 * @const
 */
const FEEDBACK_MIN_HEIGHT_LOGIN = 482;

/** @type {number}
 * @const
 */
const CONTENT_MARGIN_HEIGHT = 40;

/** @type {number}
 * @const
 */
const MAX_SCREENSHOT_WIDTH = 100;

/** @type {string}
 * @const
 */
const SYSINFO_WINDOW_ID = 'sysinfo_window';

let attachedFileBlob = null;
const lastReader = null;

/**
 * Determines whether the system information associated with this instance of
 * the feedback window has been received.
 * @type {boolean}
 */
let isSystemInfoReady = false;

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
 * The callback used by the sys_info_page to receive the event that the system
 * information is ready.
 * @type {function(sysInfo)}
 */
let sysInfoPageOnSysInfoReadyCallback = null;

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
  feedbackInfo.attachedFile = null;
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
  chrome.app.window.create(
      'chrome://slow_trace/tracing.zip#' + feedbackInfo.traceId);
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
    const description = $('description-text');
    description.placeholder = loadTimeData.getString('noDescription');
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
    feedbackInfo.traceId = null;
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

  let productId = parseInt('' + feedbackInfo.productId);
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
  // We pick the width from the titlebar, which has no margins.
  let width = $('title-bar').scrollWidth;
  if (width < FEEDBACK_MIN_WIDTH) {
    width = FEEDBACK_MIN_WIDTH;
  }

  // We get the height by adding the titlebar height and the content height +
  // margins. We can't get the margins for the content-pane here by using
  // style.margin - the variable seems to not exist.
  let height = $('title-bar').scrollHeight + $('content-pane').scrollHeight +
      CONTENT_MARGIN_HEIGHT;

  let minHeight = FEEDBACK_MIN_HEIGHT;
  if (feedbackInfo.flow == chrome.feedbackPrivate.FeedbackFlow.LOGIN) {
    minHeight = FEEDBACK_MIN_HEIGHT_LOGIN;
  }
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
      if (!feedbackInfo.flow) {
        feedbackInfo.flow = chrome.feedbackPrivate.FeedbackFlow.REGULAR;
      }

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

      // No URL, file attachment, or window minimizing for login screen
      // feedback.
      if (feedbackInfo.flow == chrome.feedbackPrivate.FeedbackFlow.LOGIN) {
        $('page-url').hidden = true;
        $('attach-file-container').hidden = true;
        $('attach-file-note').hidden = true;
        $('minimize-button').hidden = true;
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

        const sysInfoUrlElement = $('sys-info-url');
        if (sysInfoUrlElement) {
          // Opens a new window showing the full anonymized system+app
          // information.
          sysInfoUrlElement.onclick = function(e) {
            e.preventDefault();
            const win = chrome.app.window.get(SYSINFO_WINDOW_ID);
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

        const histogramUrlElement = $('histograms-url');
        if (histogramUrlElement) {
          // Opens a new window showing the histogram metrics.
          setupLinkHandlers(
              histogramUrlElement, 'chrome://histograms',
              true /* useAppWindow */);
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

          const assistantLogsInfoLinkElement = $('assistant-logs-info-link');
          if (assistantLogsInfoLinkElement) {
            assistantLogsInfoLinkElement.onclick = function(e) {
              e.preventDefault();

              chrome.app.window.create(
                  '/html/assistant_logs_info.html',
                  {width: 400, height: 120, resizable: false, frame: 'none'},
                  function(appWindow) {
                    appWindow.contentWindow.onload = function() {
                      i18nTemplate.process(
                          appWindow.contentWindow.document, loadTimeData);
                    };
                  });

              assistantLogsInfoLinkElement.onauxclick = function(e) {
                e.preventDefault();
              };
            };
          }
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
