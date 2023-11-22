// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../strings.m.js';
// <if expr="chromeos_ash">
import './jelly_colors.js';

// </if>

import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {$, getRequiredElement} from 'chrome://resources/js/util.js';

import {FeedbackBrowserProxy, FeedbackBrowserProxyImpl} from './feedback_browser_proxy.js';
import {FEEDBACK_LANDING_PAGE, FEEDBACK_LANDING_PAGE_TECHSTOP, FEEDBACK_LEGAL_HELP_URL, FEEDBACK_PRIVACY_POLICY_URL, FEEDBACK_TERM_OF_SERVICE_URL, openUrlInAppWindow} from './feedback_util.js';
import {domainQuestions, questionnaireBegin, questionnaireNotification} from './questionnaire.js';
import {takeScreenshot} from './take_screenshot.js';

declare global {
  interface Window {
    whenTestSetupDoneResolver?: {promise: Promise<void>};
  }
}

const formOpenTime: number = new Date().getTime();

/**
 * The object will be manipulated by sendReport().
 */
let feedbackInfo: chrome.feedbackPrivate.FeedbackInfo = {
  assistantDebugInfoAllowed: false,
  attachedFile: undefined,
  attachedFileBlobUuid: undefined,
  autofillMetadata: '',
  categoryTag: undefined,
  description: '...',
  descriptionPlaceholder: undefined,
  email: undefined,
  flow: chrome.feedbackPrivate.FeedbackFlow.REGULAR,
  fromAssistant: false,
  fromAutofill: false,
  includeBluetoothLogs: false,
  pageUrl: undefined,
  sendHistograms: undefined,
  systemInformation: [],
  useSystemWindowFrame: false,
  isOffensiveOrUnsafe: undefined,
  aiMetadata: undefined,
};


async function sendFeedbackReport(useSystemInfo: boolean) {
  const ID = Math.round(Date.now() / 1000);
  const FLOW = feedbackInfo.flow;

  const result = await FeedbackBrowserProxyImpl.getInstance().sendFeedback(
      feedbackInfo, useSystemInfo, formOpenTime);

  if (result.status === chrome.feedbackPrivate.Status.SUCCESS) {
    if (FLOW !== chrome.feedbackPrivate.FeedbackFlow.LOGIN &&
        result.landingPageType !==
            chrome.feedbackPrivate.LandingPageType.NO_LANDING_PAGE) {
      const landingPage = result.landingPageType ===
              chrome.feedbackPrivate.LandingPageType.NORMAL ?
          FEEDBACK_LANDING_PAGE :
          FEEDBACK_LANDING_PAGE_TECHSTOP;
      OpenWindowProxyImpl.getInstance().openUrl(landingPage);
    }
  } else {
    console.warn(
        'Feedback: Report for request with ID ' + ID + ' will be sent later.');
  }
  scheduleWindowClose();
}

let browserProxy: FeedbackBrowserProxy;

const MAX_ATTACH_FILE_SIZE: number = 3 * 1024 * 1024;

const MAX_SCREENSHOT_WIDTH: number = 100;

let attachedFileBlob: Blob|null = null;

/**
 * Which questions have been appended to the issue description text area.
 */
const appendedQuestions: {[key: string]: boolean} = {};

/**
 * Builds a RegExp that matches one of the given words. Each word has to match
 * at word boundary and is not at the end of the tested string. For example,
 * the word "SIM" would match the string "I have a sim card issue" but not
 * "I have a simple issue" nor "I have a sim" (because the user might not have
 * finished typing yet).
 * @param words The words to match.
 */
function buildWordMatcher(words: string[]): RegExp {
  return new RegExp(
      words.map((word) => '\\b' + word + '\\b[^$]').join('|'), 'i');
}

/**
 * Regular expression to check for all variants of blu[e]toot[h] with or without
 * space between the words; for BT when used as an individual word, or as two
 * individual characters, and for BLE, BlueZ, and Floss when used as an
 * individual word. Case insensitive matching.
 */
const btRegEx: RegExp = new RegExp(
    'blu[e]?[ ]?toot[h]?|\\bb[ ]?t\\b|\\bble\\b|\\bfloss\\b|\\bbluez\\b', 'i');

/**
 * Regular expression to check for wifi-related keywords.
 */
const wifiRegEx: RegExp =
    buildWordMatcher(['wifi', 'wi-fi', 'internet', 'network', 'hotspot']);

/**
 * Regular expression to check for cellular-related keywords.
 */
const cellularRegEx: RegExp = buildWordMatcher([
  '2G',   '3G',    '4G',      '5G',       'LTE',      'UMTS',
  'SIM',  'eSIM',  'mmWave',  'mobile',   'APN',      'IMEI',
  'IMSI', 'eUICC', 'carrier', 'T.Mobile', 'TMO',      'Verizon',
  'VZW',  'AT&T',  'MVNO',    'pin.lock', 'cellular',
]);

/**
 * Regular expression to check for display-related keywords.
 */
const displayRegEx = buildWordMatcher([
  'display',
  'displayport',
  'hdmi',
  'monitor',
  'panel',
  'screen',
]);

/**
 * Regular expression to check for USB-related keywords.
 */
const usbRegEx = buildWordMatcher([
  'USB',
  'USB-C',
  'Type-C',
  'TypeC',
  'USBC',
  'USBTypeC',
  'USBPD',
  'hub',
  'charger',
  'dock',
]);

/**
 * Regular expression to check for thunderbolt-related keywords.
 */
const thunderboltRegEx = buildWordMatcher([
  'Thunderbolt',
  'Thunderbolt3',
  'Thunderbolt4',
  'TBT',
  'TBT3',
  'TBT4',
  'TB3',
  'TB4',
]);

/**
 * Regular expression to check for Audio-related keywords.
 */
 const audioRegEx = buildWordMatcher([
  'audio',
  'sound',
  'mic',
  'speaker',
  'headphone',
  'headset',
  'recording',
  'volume',
  'earbud',
]);

/**
 * Regular expression to check for all strings indicating that a user can't
 * connect to a HID or Audio device. This is also a likely indication of a
 * Bluetooth related issue.
 * Sample strings this will match:
 * "I can't connect the speaker!",
 * "The keyboard has connection problem."
 */
const cantConnectRegEx: RegExp = new RegExp(
    '((headphone|keyboard|mouse|speaker)((?!(connect|pair)).*)(connect|pair))' +
        '|((connect|pair).*(headphone|keyboard|mouse|speaker))',
    'i');

/**
 * Regular expression to check for "tether" or "tethering". Case insensitive
 * matching.
 */
const tetherRegEx: RegExp = new RegExp('tether(ing)?', 'i');

/**
 * Regular expression to check for "Smart (Un)lock" or "Easy (Un)lock" with or
 * without space between the words. Case insensitive matching.
 */
const smartLockRegEx: RegExp = new RegExp('(smart|easy)[ ]?(un)?lock', 'i');

/**
 * Regular expression to check for keywords related to Nearby Share like
 * "nearby (share)" or "phone (hub)".
 * Case insensitive matching.
 */
const nearbyShareRegEx: RegExp = new RegExp('nearby|phone', 'i');

/**
 * Regular expression to check for keywords related to Fast Pair like
 * "fast pair".
 * Case insensitive matching.
 */
const fastPairRegEx: RegExp = new RegExp('fast[ ]?pair', 'i');

/**
 * Regular expression to check for Bluetooth device specific keywords.
 */
const btDeviceRegEx =
    buildWordMatcher(['apple', 'allegro', 'pixelbud', 'microsoft', 'sony']);

/**
 * Reads the selected file when the user selects a file.
 * @param fileSelectedEvent The onChanged event for the file input box.
 */
function onFileSelected(fileSelectedEvent: Event) {
  // <if expr="chromeos_ash">
  // This is needed on CrOS. Otherwise, the feedback window will stay behind
  // the Chrome window.
  browserProxy.showDialog();
  // </if>

  const file = (fileSelectedEvent.target as HTMLInputElement).files![0];
  if (!file) {
    // User canceled file selection.
    attachedFileBlob = null;
    return;
  }

  if (file.size > MAX_ATTACH_FILE_SIZE) {
    getRequiredElement('attach-error').hidden = false;

    // Clear our selected file.
    getRequiredElement<HTMLInputElement>('attach-file').value = '';
    attachedFileBlob = null;
    return;
  }

  attachedFileBlob = file.slice();
}

/**
 * Called when user opens the file dialog. Hide 'attach-error' before file
 * dialog is open to prevent a11y bug https://crbug.com/1020047
 */
function onOpenFileDialog() {
  getRequiredElement('attach-error').hidden = true;
}

/**
 * Clears the file that was attached to the report with the initial request.
 * Instead we will now show the attach file button in case the user wants to
 * attach another file.
 */
function clearAttachedFile() {
  getRequiredElement('custom-file-container').hidden = true;
  attachedFileBlob = null;
  feedbackInfo.attachedFile = undefined;
  getRequiredElement('attach-file').hidden = false;
}

/**
 * Sets up the event handlers for the given |anchorElement|.
 * @param anchorElement The <a> html element.
 * @param url The destination URL for the link.
 * @param useAppWindow true if the URL should be opened inside a new App Window,
 *     false if it should be opened in a new tab.
 */
function setupLinkHandlers(
    anchorElement: HTMLElement, url: string, useAppWindow: boolean) {
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

// <if expr="chromeos_ash">
/**
 * Opens a new window with chrome://slow_trace, downloading performance data.
 */
function openSlowTraceWindow() {
  window.open('chrome://slow_trace/tracing.zip#' + feedbackInfo.traceId);
}
// </if>

/**
 * Checks if any keywords related to bluetooth have been typed. If they are,
 * we show the bluetooth logs option, otherwise hide it.
 * @param inputEvent The input event for the description textarea.
 */
function checkForSendBluetoothLogs(inputEvent: Event) {
  const value = (inputEvent.target as HTMLInputElement).value;
  const isRelatedToBluetooth = btRegEx.test(value) ||
      cantConnectRegEx.test(value) || tetherRegEx.test(value) ||
      smartLockRegEx.test(value) || nearbyShareRegEx.test(value) ||
      fastPairRegEx.test(value) || btDeviceRegEx.test(value);
  getRequiredElement('bluetooth-checkbox-container').hidden =
      !isRelatedToBluetooth;
}

/**
 * Checks if any keywords have associated questionnaire in a domain. If so,
 * we append the questionnaire in getRequiredElement('description-text').
 * @param inputEvent The input event for the description textarea.
 */
function checkForShowQuestionnaire(inputEvent: Event) {
  const toAppend = [];

  // Match user-entered description before the questionnaire to reduce false
  // positives due to matching the questionnaire questions and answers.
  const value = (inputEvent.target as HTMLInputElement).value;
  const questionnaireBeginPos = value.indexOf(questionnaireBegin);
  const matchedText = questionnaireBeginPos >= 0 ?
      value.substring(0, questionnaireBeginPos) :
      value;

  if (btRegEx.test(matchedText)) {
    toAppend.push(...domainQuestions['bluetooth']);
  }

  if (wifiRegEx.test(matchedText)) {
    toAppend.push(...domainQuestions['wifi']);
  }

  if (cellularRegEx.test(matchedText)) {
    toAppend.push(...domainQuestions['cellular']);
  }

  if (displayRegEx.test(matchedText)) {
    toAppend.push(...domainQuestions['display']);
  }

  if (audioRegEx.test(matchedText)) {
    toAppend.push(...domainQuestions['audio']);
  }

  if (thunderboltRegEx.test(matchedText)) {
    toAppend.push(...domainQuestions['thunderbolt']);
  } else if (usbRegEx.test(matchedText)) {
    toAppend.push(...domainQuestions['usb']);
  }

  if (toAppend.length === 0) {
    return;
  }

  const textarea = getRequiredElement<HTMLTextAreaElement>('description-text');
  const savedCursor = textarea.selectionStart;
  if (Object.keys(appendedQuestions).length === 0) {
    textarea.value += '\n\n' + questionnaireBegin + '\n';
    getRequiredElement('questionnaire-notification').textContent =
        questionnaireNotification;
  }

  for (const question of toAppend) {
    if (question in appendedQuestions) {
      continue;
    }

    textarea.value += '* ' + question + ' \n';
    appendedQuestions[question] = true;
  }

  // After appending text, the web engine automatically moves the cursor to the
  // end of the appended text, so we need to move the cursor back to where the
  // user was typing before.
  textarea.selectionEnd = savedCursor;
}

/**
 * Updates the description-text box based on whether it was valid.
 * If invalid, indicate an error to the user. If valid, remove indication of the
 * error.
 */
function updateDescription(wasValid: boolean) {
  // Set visibility of the alert text for users who don't use a screen
  // reader.
  getRequiredElement('description-empty-error').hidden = wasValid;

  // Change the textarea's aria-labelled by to ensure the screen reader does
  // (or doesn't) read the error, as appropriate.
  // If it does read the error, it should do so _before_ it reads the normal
  // description.
  const description = getRequiredElement('description-text');
  description.setAttribute(
      'aria-labelledby',
      (wasValid ? '' : 'description-empty-error ') + 'free-form-text');
  // Indicate whether input is valid.
  description.setAttribute('aria-invalid', String(!wasValid));
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
 * @return Whether the report was sent.
 */
function sendReport(): boolean {
  const textarea = getRequiredElement<HTMLTextAreaElement>('description-text');
  if (textarea.value.length === 0) {
    updateDescription(false);
    return false;
  }
  // This isn't strictly necessary, since if we get past this point we'll
  // succeed, but for future-compatibility (and in case we later add more
  // failure cases after this), re-hide the alert and reset the aria label.
  updateDescription(true);

  // Prevent double clicking from sending additional reports.
  getRequiredElement<HTMLButtonElement>('send-report-button').disabled = true;
  if (!feedbackInfo.attachedFile && attachedFileBlob) {
    feedbackInfo.attachedFile = {
      name: getRequiredElement<HTMLInputElement>('attach-file').value,
      data: attachedFileBlob,
    };
  }

  const consentCheckboxValue: boolean =
      getRequiredElement<HTMLInputElement>('consent-checkbox').checked;
  feedbackInfo.systemInformation = [
    {
      key: 'feedbackUserCtlConsent',
      value: String(consentCheckboxValue),
    },
  ];

  if (feedbackInfo.flow === chrome.feedbackPrivate.FeedbackFlow.AI) {
    feedbackInfo.isOffensiveOrUnsafe =
        getRequiredElement<HTMLInputElement>('offensive-checkbox').checked;
    if (!getRequiredElement<HTMLInputElement>('log-id-checkbox').checked) {
      feedbackInfo.aiMetadata = undefined;
    }
  }

  feedbackInfo.description = textarea.value;
  feedbackInfo.pageUrl =
      getRequiredElement<HTMLInputElement>('page-url-text').value;
  feedbackInfo.email =
      getRequiredElement<HTMLSelectElement>('user-email-drop-down').value;

  let useSystemInfo = false;
  let useHistograms = false;
  const checkbox = $<HTMLInputElement>('sys-info-checkbox');
  if (checkbox != null && checkbox.checked) {
    // Send histograms along with system info.
    useHistograms = true;
    useSystemInfo = true;
  }

  const autofillCheckbox = $<HTMLInputElement>('autofill-metadata-checkbox');
  if (autofillCheckbox != null && autofillCheckbox.checked &&
      !getRequiredElement('autofill-checkbox-container').hidden) {
    feedbackInfo.sendAutofillMetadata = true;
  }

  // <if expr="chromeos_ash">
  const assistantCheckbox = $<HTMLInputElement>('assistant-info-checkbox');
  if (assistantCheckbox != null && assistantCheckbox.checked &&
      !getRequiredElement('assistant-checkbox-container').hidden) {
    // User consent to link Assistant debug info on Assistant server.
    feedbackInfo.assistantDebugInfoAllowed = true;
  }

  const bluetoothCheckbox = $<HTMLInputElement>('bluetooth-logs-checkbox');
  if (bluetoothCheckbox != null && bluetoothCheckbox.checked &&
      !getRequiredElement('bluetooth-checkbox-container').hidden) {
    feedbackInfo.sendBluetoothLogs = true;
    feedbackInfo.categoryTag = 'BluetoothReportWithLogs';
  }

  const performanceCheckbox = $<HTMLInputElement>('performance-info-checkbox');
  if (performanceCheckbox == null || !performanceCheckbox.checked) {
    feedbackInfo.traceId = undefined;
  }
  // </if>

  feedbackInfo.sendHistograms = useHistograms;

  if (getRequiredElement<HTMLInputElement>('screenshot-checkbox').checked) {
    // The user is okay with sending the screenshot and tab titles.
    feedbackInfo.sendTabTitles = true;
  } else {
    // The user doesn't want to send the screenshot, so clear it.
    feedbackInfo.screenshot = undefined;
  }

  let productId: number|undefined = parseInt('' + feedbackInfo.productId, 10);
  if (isNaN(productId)) {
    // For apps that still use a string value as the |productId|, we must clear
    // that value since the API uses an integer value, and a conflict in data
    // types will cause the report to fail to be sent.
    productId = undefined;
  }
  feedbackInfo.productId = productId;

  // Request sending the report, show the landing page (if allowed)
  sendFeedbackReport(useSystemInfo);

  return true;
}

/**
 * Click listener for the cancel button.
 */
function cancel(e: Event) {
  e.preventDefault();
  scheduleWindowClose();
}

// <if expr="chromeos_ash">
/**
 * Update the page when performance feedback state is changed.
 */
function performanceFeedbackChanged() {
  const screenshotCheckbox =
      getRequiredElement<HTMLInputElement>('screenshot-checkbox');
  const fileInput = getRequiredElement<HTMLInputElement>('attach-file');

  if (getRequiredElement<HTMLInputElement>(
      'performance-info-checkbox').checked) {
    fileInput.disabled = true;
    fileInput.checked = false;

    screenshotCheckbox.disabled = true;
    screenshotCheckbox.checked = false;
  } else {
    fileInput.disabled = false;
    screenshotCheckbox.disabled = false;
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
    browserProxy.closeDialog();
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
  /**
   * Apply updates based on the received `FeedbackInfo` object.
   * @return A promise signaling that all UI updates have finished.
   */
  function applyData(feedbackInfo: chrome.feedbackPrivate.FeedbackInfo):
      Promise<void> {
    if (feedbackInfo.includeBluetoothLogs) {
      assert(
          feedbackInfo.flow ===
          chrome.feedbackPrivate.FeedbackFlow.GOOGLE_INTERNAL);
      getRequiredElement('description-text')
          .addEventListener('input', checkForSendBluetoothLogs);
    }

    if (feedbackInfo.showQuestionnaire) {
      assert(
          feedbackInfo.flow ===
          chrome.feedbackPrivate.FeedbackFlow.GOOGLE_INTERNAL);
      getRequiredElement('description-text')
          .addEventListener('input', checkForShowQuestionnaire);
    }

    if ($('assistant-checkbox-container') != null &&
        feedbackInfo.flow ===
            chrome.feedbackPrivate.FeedbackFlow.GOOGLE_INTERNAL &&
        feedbackInfo.fromAssistant) {
      getRequiredElement('assistant-checkbox-container').hidden = false;
    }

    if ($('autofill-checkbox-container') != null &&
        feedbackInfo.flow ===
            chrome.feedbackPrivate.FeedbackFlow.GOOGLE_INTERNAL &&
        feedbackInfo.fromAutofill) {
      getRequiredElement('autofill-checkbox-container').hidden = false;
    }

    getRequiredElement('description-text').textContent =
        feedbackInfo.description;
    if (feedbackInfo.descriptionPlaceholder) {
      getRequiredElement<HTMLTextAreaElement>('description-text').placeholder =
          feedbackInfo.descriptionPlaceholder;
    }
    if (feedbackInfo.pageUrl) {
      getRequiredElement<HTMLInputElement>('page-url-text').value =
          feedbackInfo.pageUrl;
    }

    const isAiFlow: boolean =
        feedbackInfo.flow === chrome.feedbackPrivate.FeedbackFlow.AI;

    if (isAiFlow) {
      getRequiredElement('free-form-text').textContent =
          loadTimeData.getString('freeFormTextAi');
      getRequiredElement('offensive-container').hidden = false;
      getRequiredElement('log-id-container').hidden = false;
    }

    const whenScreenshotUpdated = takeScreenshot().then(function(
        screenshotCanvas) {
      // We've taken our screenshot, show the feedback page without any
      // further delay.
      window.requestAnimationFrame(function() {
        resizeAppWindow();
      });

      browserProxy.showDialog();

      // Allow feedback to be sent even if the screenshot failed.
      if (!screenshotCanvas) {
        const checkbox =
            getRequiredElement<HTMLInputElement>('screenshot-checkbox');
        checkbox.disabled = true;
        checkbox.checked = false;
        return Promise.resolve();
      }

      return new Promise<void>(function(resolve) {
        screenshotCanvas.toBlob(function(blob) {
          const image =
              getRequiredElement<HTMLImageElement>('screenshot-image');
          image.src = URL.createObjectURL(blob!);
          // Only set the alt text when the src url is available, otherwise we'd
          // get a broken image picture instead. crbug.com/773985.
          image.alt = 'screenshot';
          image.classList.toggle(
              'wide-screen', image.width > MAX_SCREENSHOT_WIDTH);
          feedbackInfo.screenshot = blob!;
          resolve();
        });
      });
    });

    const whenEmailUpdated = isAiFlow ?
        Promise.resolve() :
        browserProxy.getUserEmail().then(function(email) {
          // Never add an empty option.
          if (!email) {
            return;
          }
          const optionElement = document.createElement('option');
          optionElement.value = email;
          optionElement.text = email;
          optionElement.selected = true;
          // Make sure the "Report anonymously" option comes last.
          getRequiredElement('user-email-drop-down')
              .insertBefore(
                  optionElement, getRequiredElement('anonymous-user-option'));

          // Now we can unhide the user email section:
          getRequiredElement('user-email').hidden = false;
          // Only show email consent checkbox when an email address exists.
          getRequiredElement('consent-container').hidden = false;
        });

    // An extension called us with an attached file.
    if (feedbackInfo.attachedFile) {
      getRequiredElement('attached-filename-text').textContent =
          feedbackInfo.attachedFile.name;
      attachedFileBlob = feedbackInfo.attachedFile.data!;
      getRequiredElement('custom-file-container').hidden = false;
      getRequiredElement('attach-file').hidden = true;
    }

    // No URL, file attachment for login screen feedback.
    if (feedbackInfo.flow === chrome.feedbackPrivate.FeedbackFlow.LOGIN) {
      getRequiredElement('page-url').hidden = true;
      getRequiredElement('attach-file-container').hidden = true;
      getRequiredElement('attach-file-note').hidden = true;
    }

    // <if expr="chromeos_ash">
    if (feedbackInfo.traceId && ($('performance-info-area'))) {
      getRequiredElement('performance-info-area').hidden = false;
      getRequiredElement<HTMLInputElement>(
          'performance-info-checkbox').checked = true;
      performanceFeedbackChanged();
      getRequiredElement('performance-info-link').onclick = openSlowTraceWindow;
    }
    // </if>

    const autofillMetadataUrlElement = $('autofill-metadata-url');

    if (autofillMetadataUrlElement) {
      // Opens a new window showing the full anonymized autofill metadata.
      autofillMetadataUrlElement.onclick = function(e) {
        e.preventDefault();

        browserProxy.showAutofillMetadataInfo(feedbackInfo.autofillMetadata!);
      };

      autofillMetadataUrlElement.onauxclick = function(e) {
        e.preventDefault();
      };
    }

    const sysInfoUrlElement = $('sys-info-url');
    if (sysInfoUrlElement) {
      // Opens a new window showing the full anonymized system+app
      // information.
      sysInfoUrlElement.onclick = function(e) {
        e.preventDefault();

        browserProxy.showSystemInfo();
      };

      sysInfoUrlElement.onauxclick = function(e) {
        e.preventDefault();
      };
    }

    const histogramUrlElement = $('histograms-url');
    if (histogramUrlElement) {
      histogramUrlElement.onclick = function(e) {
        e.preventDefault();

        browserProxy.showMetrics();
      };

      histogramUrlElement.onauxclick = function(e) {
        e.preventDefault();
      };
    }

    // The following URLs don't open on login screen, so hide them.
    // TODO(crbug.com/1116383): Find a solution to display them properly.
    // Update: the bluetooth and assistant logs links will work on login
    // screen now. But to limit the scope of this CL, they are still hidden.
    if (feedbackInfo.flow !== chrome.feedbackPrivate.FeedbackFlow.LOGIN) {
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

      // <if expr="chromeos_ash">
      const bluetoothLogsInfoLinkElement = $('bluetooth-logs-info-link');
      if (bluetoothLogsInfoLinkElement) {
        bluetoothLogsInfoLinkElement.onclick = function(e) {
          e.preventDefault();

          browserProxy.showBluetoothLogsInfo();

          bluetoothLogsInfoLinkElement.onauxclick = function(e) {
            e.preventDefault();
          };
        };
      }

      const assistantLogsInfoLinkElement = $('assistant-logs-info-link');
      if (assistantLogsInfoLinkElement) {
        assistantLogsInfoLinkElement.onclick = function(e) {
          e.preventDefault();

          browserProxy.showAssistantLogsInfo();

          assistantLogsInfoLinkElement.onauxclick = function(e) {
            e.preventDefault();
          };
        };
      }
      // </if>
    }

    // Make sure our focus starts on the description field.
    getRequiredElement('description-text').focus();

    return Promise.all([whenScreenshotUpdated, whenEmailUpdated])
        .then(() => {});
  }

  window.addEventListener('DOMContentLoaded', async function() {
    if (window.whenTestSetupDoneResolver) {
      // Hook for tests to perform setup steps before any other code runs.
      await window.whenTestSetupDoneResolver.promise;
    }

    // Initialize `browserProxy` only after tests had a chance to do setup
    // steps, one of which is to replace the prod proxy with a test version.
    browserProxy = FeedbackBrowserProxyImpl.getInstance();

    const dialogArgs = browserProxy.getDialogArguments();
    if (dialogArgs) {
      feedbackInfo = JSON.parse(dialogArgs);
    }

    await applyData(feedbackInfo);

    // Setup our event handlers.
    getRequiredElement('attach-file').addEventListener(
        'change', onFileSelected);
    getRequiredElement('attach-file').addEventListener(
        'click', onOpenFileDialog);
    getRequiredElement('send-report-button').onclick = sendReport;
    getRequiredElement('cancel-button').onclick = cancel;
    getRequiredElement('remove-attached-file').onclick = clearAttachedFile;
    // <if expr="chromeos_ash">
    getRequiredElement('performance-info-checkbox')
        .addEventListener('change', performanceFeedbackChanged);
    // </if>

    // Dispatch event used by tests.
    document.documentElement.dispatchEvent(
        new CustomEvent('ready-for-testing'));
  });
}

initialize();
