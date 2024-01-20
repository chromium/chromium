// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertExists} from './assert.js';
import * as dom from './dom.js';
import {reportError} from './error.js';
import {Flag} from './flag.js';
import {I18nString} from './i18n_string.js';
import {BarcodeContentType, sendBarcodeDetectedEvent} from './metrics.js';
import * as loadTimeData from './models/load_time_data.js';
import {ChromeHelper} from './mojo/chrome_helper.js';
import {
  WifiConfig,
  WifiEapMethod,
  WifiEapPhase2Method,
  WifiSecurityType,
} from './mojo/type.js';
import * as snackbar from './snackbar.js';
import * as state from './state.js';
import {OneShotTimer} from './timer.js';
import {
  ErrorLevel,
  ErrorType,
} from './type.js';

const QR_CODE_ESCAPE_CHARS = ['\\', ';', ',', ':'];

// TODO(b/172879638): Tune the duration according to the final motion spec.
const CHIP_DURATION = 8000;

/**
 * The detected string that is being shown currently.
 */
let currentCode: string|null = null;

/**
 * The barcode chip container that is being shown currently.
 */
let currentChip: HTMLElement|null = null;

/**
 * The countdown timer for dismissing the chip.
 */
let currentTimer: OneShotTimer|null = null;

/**
 * Resets the variables of the current state and dismisses the chip.
 */
function deactivate() {
  if (currentChip !== null) {
    currentChip.classList.add('invisible');
  }
  currentCode = null;
  currentChip = null;
  currentTimer = null;
}

/**
 * Activates the chip on container and starts the timer.
 *
 * @param container The container of the chip.
 */
function activate(container: HTMLElement) {
  container.classList.remove('invisible');
  currentChip = container;

  currentTimer = new OneShotTimer(deactivate, CHIP_DURATION);
  if (state.get(state.State.KEYBOARD_NAVIGATION)) {
    // Do not auto dismiss the chip when using keyboard for a11y. Screen reader
    // might need long time to read the detected content.
    currentTimer.stop();
  }
}

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
  let securityType = 'nopass';
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
            securityType = val;
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
  sendBarcodeDetectedEvent(
      {contentType: BarcodeContentType.WIFI}, securityType);
  if (!['WEP', 'WPA', 'WPA2-EAP', 'nopass'].includes(securityType)) {
    return null;
  } else if (securityType === 'nopass') {
    return {
      ssid: ssid,
      security: WifiSecurityType.kNone,
    };
  } else if (password === null) {
    return null;
  } else if (securityType === 'WEP') {
    return {
      ssid: ssid,
      security: WifiSecurityType.kWep,
      password: password,
    };
  } else if (securityType === 'WPA') {
    return {
      ssid: ssid,
      security: WifiSecurityType.kWpa,
      password: password,
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
 * @param container The container for the button.
 * @param content The content to be copied.
 * @param snackbarLabel The label to be displayed on snackbar when the content
 *     is copied.
 */
function createCopyButton(
    container: HTMLElement, content: string,
    snackbarLabel: I18nString): HTMLElement {
  const copyButton =
      dom.getFrom(container, '.barcode-copy-button', HTMLButtonElement);
  copyButton.onclick = async () => {
    await navigator.clipboard.writeText(content);
    snackbar.show(snackbarLabel);
  };
  return copyButton;
}

/**
 * Shows an actionable url chip.
 */
function showUrl(url: string) {
  const container = dom.get('#barcode-chip-url-container', HTMLDivElement);
  activate(container);

  const textEl = dom.get('#barcode-chip-url-content', HTMLSpanElement);
  textEl.textContent =
      loadTimeData.getI18nMessage(I18nString.BARCODE_LINK_CHIPTEXT, url);

  const chip = dom.get('#barcode-chip-url', HTMLButtonElement);
  chip.onclick = () => {
    ChromeHelper.getInstance().openUrlInBrowser(url);
  };
  chip.focus();

  const copyButton =
      createCopyButton(container, url, I18nString.SNACKBAR_LINK_COPIED);
  const label =
      loadTimeData.getI18nMessage(I18nString.BARCODE_COPY_LINK_BUTTON, url);
  copyButton.setAttribute('aria-label', label);
}

/**
 * Shows an actionable text chip.
 */
function showText(text: string) {
  const container = dom.get('#barcode-chip-text-container', HTMLDivElement);
  activate(container);

  const textEl = dom.get('#barcode-chip-text-content', HTMLSpanElement);
  textEl.textContent = text;
  const expandable = textEl.scrollWidth > textEl.clientWidth;

  const expandEl = dom.get('#barcode-chip-text-expand', HTMLButtonElement);
  expandEl.classList.toggle('hidden', !expandable);
  expandEl.onclick = () => {
    container.classList.toggle('expanded');
    const expanded = container.classList.contains('expanded');
    expandEl.setAttribute('aria-expanded', expanded.toString());
  };

  const copyButton =
      createCopyButton(container, text, I18nString.SNACKBAR_TEXT_COPIED);
  const label =
      loadTimeData.getI18nMessage(I18nString.BARCODE_COPY_TEXT_BUTTON, text);
  copyButton.setAttribute('aria-label', label);

  // TODO(b/172879638): There is a race in ChromeVox which will speak the
  // focused element twice.
  copyButton.focus();
}

/**
 * Shows an actionable wifi chip for connecting Wi-fi.
 */
function showWifi(wifiConfig: WifiConfig) {
  const container = dom.get('#barcode-chip-wifi-container', HTMLDivElement);
  activate(container);

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

  chip.focus();
}

/**
 * Shows an actionable chip for the string detected from a barcode.
 */
export function show(code: string): void {
  if (code === currentCode) {
    if (currentTimer !== null) {
      // Extend the duration by resetting the timeout.
      currentTimer.resetTimeout();
    }
    return;
  }

  if (currentTimer !== null) {
    // Dismiss the previous chip.
    currentTimer.fireNow();
    assert(currentTimer === null, 'The timer should be cleared.');
  }

  currentCode = code;
  const wifiConfig = parseWifi(code);
  if (loadTimeData.getChromeFlag(Flag.AUTO_QR) && wifiConfig !== null) {
    showWifi(wifiConfig);
  } else if (isSafeUrl(code)) {
    sendBarcodeDetectedEvent({contentType: BarcodeContentType.URL});
    showUrl(code);
  } else {
    sendBarcodeDetectedEvent({contentType: BarcodeContentType.TEXT});
    showText(code);
  }
}

/**
 * Dismisses the current barcode chip if it's being shown.
 */
export function dismiss(): void {
  if (currentTimer === null) {
    return;
  }
  currentTimer.fireNow();
}
