// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';

import {KeyboardUtils} from './keyboard_utils.js';

// This global instance of KeyboardUtils is initialized by `display_manager`
// when OOBE has keyboard navigation enabled. It seems that this is only useful
// for Google Meet Devices. Formerly known as CfM (Chromebox for Meetings), aka
// 'remora' devices.
export const globalOobeKeyboard = new KeyboardUtils();


/**
 * Code which is embedded inside of the enterprise enrollment webview.
 * See /screens/oobe/enterprise_enrollment.js for details.
 */
export const KEYBOARD_UTILS_FOR_INJECTION = {
  DATA: '',
};

function prepareKeyboardUtilsForInjection(sourceCode: string): void {
  assert(typeof sourceCode.replaceAll === 'function');

  // Remove exports. 'export class' -> 'class'
  const srcWithoutExports = sourceCode.replaceAll('export class', 'class');

  const finalSourceCode = `
    (function() {
      ${srcWithoutExports};
      window.InjectedKeyboard = new InjectedKeyboardUtils();
      window.InjectedKeyboard.initializeKeyboardFlow();
    })();
    `;
  KEYBOARD_UTILS_FOR_INJECTION.DATA = finalSourceCode;
}

function fetchKeyboardUtilsSource(): void {
  const keyboardUtilsUrl = 'components/keyboard_utils.js';
  const xhr = new XMLHttpRequest();
  xhr.responseType = 'text';
  xhr.onreadystatechange = function() {
    if (xhr.readyState === 4 /* DONE */) {
      assert(200 === xhr.status);
      assert(xhr.responseType === 'text');
      prepareKeyboardUtilsForInjection(xhr.responseText);
    }
  };
  xhr.open('GET', keyboardUtilsUrl, true);
  xhr.send();
}

fetchKeyboardUtilsSource();
