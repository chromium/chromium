// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';

// <if expr="enable_pdf_ink2">
import type {Color} from './constants.js';
// </if>
import type {LayoutOptions, ViewportRect} from './viewport.js';

// <if expr="enable_pdf_ink2">
// LINT.IfChange(HighlighterOpacity)
const HIGHLIGHTER_OPACITY: number = 0.4;
// LINT.ThenChange(//pdf/pdf_ink_brush.cc:HighlighterOpacity)
// </if>

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
 * Determines if the event has the platform-equivalent of the Windows ctrl key
 * modifier, and only that modifier.
 * @return Whether the event only has the ctrl key modifier.
 */
export function hasCtrlModifierOnly(e: KeyboardEvent): boolean {
  let metaModifier = e.metaKey;
  // <if expr="is_macosx">
  metaModifier = e.ctrlKey;
  // </if>
  return hasCtrlModifier(e) && !e.shiftKey && !e.altKey && !metaModifier;
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

// <if expr="enable_pdf_ink2">
/**
 * Blends `colorValue` with highlighter opacity on a white background.
 * @param colorValue The red, green, or blue value of a color.
 * @returns The new respective red, green, or blue value of a color that has
 * been transformed using the highlighter transparency on a white background.
 */
export function blendHighlighterColorValue(colorValue: number): number {
  return Math.round(
      colorValue * HIGHLIGHTER_OPACITY + 255 * (1 - HIGHLIGHTER_OPACITY));
}

/**
 * @param color The `Color` in RGB values.
 * @returns A hex-coded color string, formatted as '#ffffff'.
 */
export function colorToHex(color: Color): string {
  const rgb = [color.r, color.g, color.b]
                  .map(value => value.toString(16).padStart(2, '0'))
                  .join('');
  return `#${rgb}`;
}

/**
 * @param hex A hex-coded color string, formatted as '#ffffff'.
 * @returns The `Color` in RGB values.
 */
export function hexToColor(hex: string): Color {
  assert(/^#[0-9a-f]{6}$/.test(hex));

  return {
    r: Number.parseInt(hex.substring(1, 3), 16),
    g: Number.parseInt(hex.substring(3, 5), 16),
    b: Number.parseInt(hex.substring(5, 7), 16),
  };
}
// </if>
