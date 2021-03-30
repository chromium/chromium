// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';

import {MessageData} from './controller.js';
import {LayoutOptions} from './viewport.js';

/**
 * @typedef {{
 *   source: Object,
 *   origin: string,
 *   data: !MessageData,
 * }}
 */
export let MessageObject;

/**
 * @typedef {{
 *   type: string,
 *   height: number,
 *   width: number,
 *   layoutOptions: (!LayoutOptions|undefined),
 *   pageDimensions: Array
 * }}
 */
export let DocumentDimensionsMessageData;

/**
 * @typedef {{
 *   type: string,
 *   page: number,
 *   x: number,
 *   y: number,
 *   zoom: number
 * }}
 */
export let DestinationMessageData;

/**
 * @typedef {{
 *   fileName: string,
 *   dataToSave: !ArrayBuffer
 * }}
 */
export let RequiredSaveResult;

/**
 * Whether keydown events should currently be ignored. Events are ignored when
 * an editable element has focus, to allow for proper editing controls.
 * @return {boolean} True if keydown events should be ignored.
 */
export function shouldIgnoreKeyEvents() {
  const activeElement = getDeepActiveElement();
  return activeElement.isContentEditable ||
      (activeElement.tagName === 'INPUT' && activeElement.type !== 'radio') ||
      activeElement.tagName === 'TEXTAREA';
}
