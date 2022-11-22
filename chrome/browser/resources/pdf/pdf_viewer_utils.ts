// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert_ts.js';
import {getDeepActiveElement} from 'chrome://resources/js/util_ts.js';

import {LayoutOptions, ViewportRect} from './viewport.js';

export interface DocumentDimensionsMessageData {
  type: string;
  height: number;
  width: number;
  pageDimensions: ViewportRect[];
  layoutOptions?: LayoutOptions;
}

export interface DestinationMessageData {
  type: string;
  page: number;
  x: number;
  y: number;
  zoom: number;
}

export interface RequiredSaveResult {
  fileName: string;
  dataToSave: ArrayBuffer;
}

/**
 * Determines if the event has the platform-equivalent of the Windows ctrl key
 * modifier.
 * @return Whether the event has the ctrl key modifier.
 */
export function hasCtrlModifier(e: KeyboardEvent): boolean {
  let hasModifier = e.ctrlKey;
  // <if expr="is_macosx">
  hasModifier = e.metaKey;  // AKA Command.
  // </if>
  return hasModifier;
}

/**
 * Whether keydown events should currently be ignored. Events are ignored when
 * an editable element has focus, to allow for proper editing controls.
 * @return Whether keydown events should be ignored.
 */
export function shouldIgnoreKeyEvents(): boolean {
  const activeElement = getDeepActiveElement();
  assert(activeElement);
  return (activeElement as HTMLElement).isContentEditable ||
      (activeElement.tagName === 'INPUT' &&
       (activeElement as HTMLInputElement).type !== 'radio') ||
      activeElement.tagName === 'TEXTAREA';
}
