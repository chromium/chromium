// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertExists, checkEnumVariant} from './assert.js';
import {showPreviewOCRToast} from './custom_effect.js';
import * as dom from './dom.js';
import {reportError} from './error.js';
import {I18nString} from './i18n_string.js';
import {
  BarcodeContentType,
  OcrEventType,
  sendBarcodeDetectedEvent,
  sendOcrEvent,
  sendUnsupportedProtocolEvent,
} from './metrics.js';
import * as loadTimeData from './models/load_time_data.js';
import * as localStorage from './models/local_storage.js';
import {ChromeHelper} from './mojo/chrome_helper.js';
import {
  OcrResult,
  WifiConfig,
  WifiEapMethod,
  WifiEapPhase2Method,
  WifiSecurityType,
} from './mojo/type.js';
import {isTopMostView} from './nav.js';
import * as snackbar from './snackbar.js';
import {speak} from './spoken_msg.js';
import * as state from './state.js';
import {OneShotTimer} from './timer.js';
import {
  ErrorLevel,
  ErrorType,
  LocalStorageKey,
  ViewName,
} from './type.js';
import {getKeyboardShortcut} from './util.js';

// Supported source types.
export enum Source {
  BARCODE = 'BARCODE',
  OCR = 'OCR',
}

interface ChipMethods {
  // Hide the chip element.
  hide(): void;
  // Focus the chip. The element receiving focus depends on the type of chip.
  focus(): void;
  // Returns if the chip is expanded.
  isExpanded?(): boolean;
  // Returns if the chip is focused inside.
  isFocused?(): boolean;
}

interface CurrentChip extends ChipMethods {
  // The detected string that is being shown currently.
  content: string;
  // The type of scanner that detected the content.
  source: Source;
  // The countdown timer for dismissing the chip.
  timer: OneShotTimer;
}

let currentChip: CurrentChip|null = null;

export enum SupportedWifiSecurityType {
  EAP = 'WPA2-EAP',
  NONE = 'nopass',
  WEP = 'WEP',
  WPA = 'WPA',
}

const QR_CODE_ESCAPE_CHARS = ['\\', ';', ',', ':'];

// TODO(b/172879638): Tune the duration according to the final motion spec.
const CHIP_DURATION = 8000;
// Screen reader users may take longer to read content. We treat keyboard users
// as screen reader users since we can't tell the difference. This is the
// maximum possible delay for setTimeout, preventing the timeout from firing.
const CHIP_DURATION_KEYBOARD = 2 ** 31 - 1;

/**
 * Checks whether a string is a regular url link with http or https protocol.
 */
function isSafeUrl(s: string): boolean {
  try {
    const url = new URL(s);
    if (url.protocol !== 'http:' && url.protocol !== 'https:') {
      reportError(
          ErrorType.UNSUPPORTED_PROTOCOL, ErrorLevel.WARNING,
          new Error(`Reject url with protocol: ${url.protocol}`));
      sendUnsupportedProtocolEvent();
      return false;
    }
    return true;
  } catch (e) {
    return false;
  }
}

/**
 * Parses the given string `s`. If the string is a wifi connection request,
 * return `WifiConfig` and if not, return null.
 */
function parseWifi(s: string): WifiConfig|null {
  let securityType: SupportedWifiSecurityType|null =
      SupportedWifiSecurityType.NONE;
  let ssid = null;
  let password = null;
  let eapMethod = null;
  let anonIdentity = null;
  let identity = null;
  let phase2method = null;

  // Example string `WIFI:S:<SSID>;P:<PASSWORD>;T:<WPA|WEP|WPA2-EAP|nopass>;H;;`
  // Reference:
  // https://github.com/zxing/zxing/wiki/Barcode-Contents#wi-fi-network-config-android-ios-11
  if (s.startsWith('WIFI:') && s.endsWith(';;')) {
    s = s.substring(5, s.length - 1);
    let i = 0;
    let component = '';
    while (i < s.length) {
      // Unescape characters escaped with a backslash
      if (s[i] === '\\' && i + 1 < s.length &&
          QR_CODE_ESCAPE_CHARS.includes(s[i + 1])) {
        component += s[i + 1];
        i += 2;
      } else if (s[i] === ';') {
        const splitIdx = component.search(':');
        if (splitIdx === -1) {
          return null;
        }
        const key = component.substring(0, splitIdx);
        const val = component.substring(splitIdx + 1);
        switch (key) {
          case 'A':
            anonIdentity = val;
            break;
          case 'E':
            eapMethod = val;
            break;
          case 'H':
            if (val !== 'true' && val !== 'false') {
              phase2method = val;
            }
            break;
          case 'I':
            identity = val;
            break;
          case 'P':
            password = val;
            break;
          case 'PH2':
            phase2method = val;
            break;
          case 'S':
            ssid = val;
            break;
          case 'T':
            securityType = checkEnumVariant(SupportedWifiSecurityType, val);
            sendBarcodeDetectedEvent(
                {contentType: BarcodeContentType.WIFI}, val);
            break;
          default:
            return null;
        }
        component = '';
        i += 1;
      } else {
        component += s[i];
        i += 1;
      }
    }
  }

  if (ssid === null) {
    return null;
  }
  if (securityType === null) {
    return null;
  } else if (securityType === SupportedWifiSecurityType.NONE) {
    return {
      ssid: ssid,
      security: WifiSecurityType.kNone,
      password: null,
      eapMethod: null,
      eapPhase2Method: null,
      eapIdentity: null,
      eapAnonymousIdentity: null,
    };
  } else if (password === null) {
    return null;
  } else if (securityType === SupportedWifiSecurityType.WEP) {
    return {
      ssid: ssid,
      security: WifiSecurityType.kWep,
      password: password,
      eapMethod: null,
      eapPhase2Method: null,
      eapIdentity: null,
      eapAnonymousIdentity: null,
    };
  } else if (securityType === SupportedWifiSecurityType.WPA) {
    return {
      ssid: ssid,
      security: WifiSecurityType.kWpa,
      password: password,
      eapMethod: null,
      eapPhase2Method: null,
      eapIdentity: null,
      eapAnonymousIdentity: null,
    };
  } else if (
      eapMethod !== null && anonIdentity !== null && identity !== null &&
      phase2method !== null) {
    const wifiEapMethod = strToWifiEapMethod(eapMethod);
    const wifiEapPhase2method = strToWifiEapPhase2Method(phase2method);

    if (wifiEapMethod !== null && wifiEapPhase2method !== null) {
      return {
        ssid: ssid,
        security: WifiSecurityType.kEap,
        password: password,
        eapMethod: wifiEapMethod,
        eapPhase2Method: wifiEapPhase2method,
        eapIdentity: identity,
        eapAnonymousIdentity: anonIdentity,
      };
    }
  }
  return null;
}

/**
 * Converts `eapMethod` to supporting WifiEapMethod. If the type is not
 * supported, return null.
 */
function strToWifiEapMethod(eapMethod: string): WifiEapMethod|null {
  if (eapMethod === 'TLS') {
    return WifiEapMethod.kEapTls;
  } else if (eapMethod === 'TTLS') {
    return WifiEapMethod.kEapTtls;
  } else if (eapMethod === 'LEAP') {
    return WifiEapMethod.kLeap;
  } else if (eapMethod === 'PEAP') {
    return WifiEapMethod.kPeap;
  }
  return null;
}

/**
 * Converts `phase2method` to supporting WifiEapPhase2Method. If the type is not
 * supported, return null.
 */
function strToWifiEapPhase2Method(phase2method: string): WifiEapPhase2Method|
    null {
  if (phase2method === 'CHAP') {
    return WifiEapPhase2Method.kChap;
  } else if (phase2method === 'GTC') {
    return WifiEapPhase2Method.kGtc;
  } else if (phase2method === 'MD5') {
    return WifiEapPhase2Method.kMd5;
  } else if (phase2method === 'MSCHAP') {
    return WifiEapPhase2Method.kMschap;
  } else if (phase2method === 'MSCHAPv2') {
    return WifiEapPhase2Method.kMschapv2;
  } else if (phase2method === 'PAP') {
    return WifiEapPhase2Method.kPap;
  } else if (phase2method === 'Automatic') {
    return WifiEapPhase2Method.kAutomatic;
  }
  return null;
}

/**
 * Creates the copy button.
 *
 * TODO(b/311592341): Rename related strings and classes since they are used by
 * both barcode and OCR.
 *
 * @param container The container for the button.
 * @param content The content to be copied.
 * @param snackbarLabel The label to be displayed on snackbar when the content
 *     is copied.
 * @param onCopy Called when the user clicks the copy button.
 */
function createCopyButton(
    container: HTMLElement, content: string, snackbarLabel: I18nString,
    onCopy?: () => void): HTMLElement {
  const copyButton =
      dom.getFrom(container, '.barcode-copy-button', HTMLButtonElement);
  copyButton.onclick = async () => {
    await navigator.clipboard.writeText(content);
    speak(I18nString.COPIED_DETECTED_CONTENT, content);
    snackbar.show(snackbarLabel);
    onCopy?.();
  };
  return copyButton;
}

/**
 * Shows an actionable url chip.
 */
function showUrl(url: string): ChipMethods {
  const container = dom.get('#barcode-chip-url-container', HTMLDivElement);
  container.classList.remove('invisible');

  const textEl = dom.get('#barcode-chip-url-content', HTMLSpanElement);
  textEl.textContent =
      loadTimeData.getI18nMessage(I18nString.BARCODE_LINK_CHIPTEXT, url);

  const chip = dom.get('#barcode-chip-url', HTMLButtonElement);
  chip.onclick = () => {
    ChromeHelper.getInstance().openUrlInBrowser(url);
  };

  const copyButton =
      createCopyButton(container, url, I18nString.SNACKBAR_LINK_COPIED);
  const label =
      loadTimeData.getI18nMessage(I18nString.BARCODE_COPY_LINK_BUTTON, url);
  copyButton.setAttribute('aria-label', label);

  return {
    hide() {
      container.classList.add('invisible');
    },
    focus() {
      copyButton.focus();
    },
  };
}

/**
 * Shows an actionable text chip.
 *
 * By default, the chip only shows a one-line preview of text. The chip can be
 * expanded to show the full content if the text is too long.
 *
 * TODO(b/311592341): Rename related strings and classes since they are used by
 * both barcode and OCR.
 */
function showText(text: string, onCopy?: () => void): ChipMethods {
  const container = dom.get('#barcode-chip-text-container', HTMLDivElement);
  const expandEl = dom.get('#barcode-chip-text-expand', HTMLButtonElement);
  const descriptionEl = dom.get('#barcode-chip-text-description', HTMLElement);

  function isChipExpanded() {
    return container.classList.contains('expanded');
  }
  function hideChip() {
    container.classList.add('invisible');
  }
  function expandChip() {
    container.classList.add('expanded');
    expandEl.ariaExpanded = 'true';
    expandEl.setAttribute(
        'aria-label',
        loadTimeData.getI18nMessage(
            I18nString.LABEL_COLLAPSE_DETECTED_CONTENT_BUTTON));
  }
  function collapseChip() {
    container.classList.remove('expanded');
    expandEl.ariaExpanded = 'false';
    expandEl.setAttribute(
        'aria-label',
        loadTimeData.getI18nMessage(
            I18nString.LABEL_EXPAND_DETECTED_CONTENT_BUTTON));
  }

  collapseChip();
  container.classList.remove('invisible');

  const textEl = dom.get('#barcode-chip-text-content', HTMLSpanElement);
  textEl.textContent = text;
  const expandable = textEl.scrollWidth > textEl.clientWidth;

  if (expandable) {
    descriptionEl.textContent = loadTimeData.getI18nMessage(
        I18nString.TEXT_DETECTED_DESCRIPTION_EXPANDABLE);
    expandEl.classList.remove('hidden');
    expandEl.onclick = () => {
      const chipTimer = assertExists(currentChip).timer;
      if (isChipExpanded()) {
        collapseChip();
        chipTimer.start();
      } else {
        expandChip();
        chipTimer.stop();
      }
    };
  } else {
    descriptionEl.textContent =
        loadTimeData.getI18nMessage(I18nString.TEXT_DETECTED_DESCRIPTION);
    expandEl.classList.add('hidden');
  }

  const copyButton = createCopyButton(
      container, text, I18nString.SNACKBAR_TEXT_COPIED, onCopy);

  return {
    hide: hideChip,
    focus() {
      // TODO(b/172879638): There is a race in ChromeVox which will speak the
      // focused element twice.
      copyButton.focus();
    },
    isExpanded: isChipExpanded,
    isFocused() {
      return container.contains(document.activeElement);
    },
  };
}

/**
 * Shows an actionable wifi chip for connecting Wi-fi.
 */
function showWifi(wifiConfig: WifiConfig): ChipMethods {
  const container = dom.get('#barcode-chip-wifi-container', HTMLDivElement);
  container.classList.remove('invisible');

  const ssidString = assertExists(wifiConfig.ssid);

  const textEl = dom.get('#barcode-chip-wifi-content', HTMLSpanElement);
  const text =
      loadTimeData.getI18nMessage(I18nString.BARCODE_WIFI_CHIPTEXT, ssidString);
  textEl.textContent = text;

  const chip = dom.get('#barcode-chip-wifi', HTMLElement);
  const label = loadTimeData.getI18nMessage(
      I18nString.LABEL_BARCODE_WIFI_CHIP, ssidString);
  chip.setAttribute('aria-label', label);
  chip.onclick = () => {
    ChromeHelper.getInstance().openWifiDialog(wifiConfig);
  };

  return {
    hide() {
      container.classList.add('invisible');
    },
    focus() {
      chip.focus();
    },
  };
}

/**
 * Show an actionable chip for content detected from barcode.
 */
export function showBarcodeContent(content: string): void {
  function setupChip() {
    let chipMethods: ChipMethods|null = null;
    const wifiConfig = parseWifi(content);
    if (wifiConfig !== null) {
      chipMethods = showWifi(wifiConfig);
    } else if (isSafeUrl(content)) {
      sendBarcodeDetectedEvent({contentType: BarcodeContentType.URL});
      chipMethods = showUrl(content);
    } else {
      sendBarcodeDetectedEvent({contentType: BarcodeContentType.TEXT});
      chipMethods = showText(content);
    }
    return assertExists(chipMethods);
  }
  showChip({
    setupChip,
    content,
    source: Source.BARCODE,
  });
}

/**
 * Show an actionable chip for content detected from OCR.
 */
export function showOcrContent(ocrResult: OcrResult): void {
  const content = ocrResult.lines.map((line) => line.text).join('\n');
  function setupChip() {
    // TODO(b/303584151): Remove the toast around 3 milestones after the feature
    // is launched.
    if (!localStorage.getBool(LocalStorageKey.PREVIEW_OCR_TOAST_SHOWN)) {
      const cameraView = dom.get('#view-camera', HTMLElement);
      showPreviewOCRToast(cameraView);
      localStorage.set(LocalStorageKey.PREVIEW_OCR_TOAST_SHOWN, true);
    }
    // TODO(b/311592341): Check if we can show Wifi and URL chip when the source
    // is OCR.
    return showText(content, () => {
      sendOcrEvent({
        eventType: OcrEventType.COPY_TEXT,
        result: ocrResult,
      });
    });
  }
  showChip({
    setupChip,
    content,
    source: Source.OCR,
  });
}

interface ShowChipParams {
  // Shows the chip and returns methods for controlling the chip.
  setupChip(): ChipMethods;
  // The detected `content` and `source` of the scanner. They are used to check
  // if the same content is being shown.
  content: string;
  source: Source;
}

/**
 * Shows an actionable chip for the string detected from various scanners.
 */
function showChip({setupChip, content, source}: ShowChipParams): void {
  const isShowing = currentChip !== null;

  if (isShowing) {
    assert(currentChip !== null);
    // Skip updating the chip if it's expanded.
    if (currentChip.isExpanded?.() === true) {
      return;
    }
    // Extend the duration by resetting the timeout if the same content is being
    // shown.
    if (currentChip.source === source && currentChip.content === content) {
      currentChip.timer.resetTimeout();
      return;
    }
  }

  dismiss();
  const chipMethods = setupChip();

  // Only focus on chip when newly shown and camera view is the top view.
  if (!isShowing && isTopMostView(ViewName.CAMERA)) {
    chipMethods.focus();
  }

  const chipDuration = state.get(state.State.KEYBOARD_NAVIGATION) ?
      CHIP_DURATION_KEYBOARD :
      CHIP_DURATION;
  const timer = new OneShotTimer(dismiss, chipDuration);

  currentChip = {
    content,
    ...chipMethods,
    source,
    timer,
  };
  window.addEventListener('keydown', onKeyDown);
}

/**
 * Dismisses the current chip if it's being shown.
 */
export function dismiss(): void {
  if (currentChip === null) {
    return;
  }
  currentChip.timer.stop();
  currentChip.hide();
  currentChip = null;
  window.removeEventListener('keydown', onKeyDown);
}

function onKeyDown(event: KeyboardEvent) {
  const {isFocused} = assertExists(currentChip);
  if (isFocused?.() === true && getKeyboardShortcut(event) === 'Escape') {
    dismiss();
  }
}
