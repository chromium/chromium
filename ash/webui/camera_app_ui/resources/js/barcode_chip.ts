// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from './assert.js';
import * as dom from './dom.js';
import {reportError} from './error.js';
import {I18nString} from './i18n_string.js';
import {BarcodeContentType, sendBarcodeDetectedEvent} from './metrics.js';
import * as loadTimeData from './models/load_time_data.js';
import {ChromeHelper} from './mojo/chrome_helper.js';
import * as snackbar from './snackbar.js';
import * as state from './state.js';
import {OneShotTimer} from './timer.js';
import {
  ErrorLevel,
  ErrorType,
} from './type.js';

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

  const anchor = dom.getFrom(container, 'a', HTMLAnchorElement);
  Object.assign(anchor, {
    textContent: url,
    onclick: () => {
      ChromeHelper.getInstance().openUrlInBrowser(url);
    },
  });
  const hostname = new URL(url).hostname;
  const label =
      loadTimeData.getI18nMessage(I18nString.BARCODE_LINK_DETECTED, hostname);
  anchor.setAttribute('aria-label', label);
  anchor.setAttribute('aria-description', url);
  anchor.focus();

  createCopyButton(container, url, I18nString.SNACKBAR_LINK_COPIED);
}

/**
 * Shows an actionable text chip.
 */
function showText(text: string) {
  const container = dom.get('#barcode-chip-text-container', HTMLDivElement);
  activate(container);
  container.classList.remove('expanded');

  const textEl = dom.get('#barcode-chip-text-content', HTMLDivElement);
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

  // TODO(b/172879638): There is a race in ChromeVox which will speak the
  // focused element twice.
  if (expandable) {
    expandEl.focus();
  } else {
    copyButton.focus();
  }
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

  if (isSafeUrl(code)) {
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
