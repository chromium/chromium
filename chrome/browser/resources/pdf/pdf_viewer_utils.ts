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

// <if expr="enable_pdf_save_to_drive">
const SAVE_TO_DRIVE_ACCOUNT_CHOOSER_URL: string =
    'https://accounts.google.com/AccountChooser';

const SAVE_TO_DRIVE_CONSUMER_MANAGE_STORAGE_URL: string =
    'https://one.google.com/storage' +
    '?utm_source=drive' +
    '&utm_medium=desktop' +
    '&utm_campaign=error_dialog_oos';

const SAVE_TO_DRIVE_DASHER_MANAGE_STORAGE_URL: string =
    'https://drive.google.com/drive/quota';

const SAVE_TO_DRIVE_DRIVE_URL: string = 'https://drive.google.com';
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

/* Verifies that the array buffer is suitable for the beginning of a PDF.
 *
 * @param buffer The beginning of data for a PDF.
 */
export function verifyPdfHeader(buffer: ArrayBuffer) {
  const MIN_FILE_SIZE = '%PDF1.0'.length;

  // Verify the first bytes to make sure it's a PDF.
  const bufView = new Uint8Array(buffer);
  assert(bufView.length >= MIN_FILE_SIZE);
  assert(
      String.fromCharCode(
          bufView[0]!, bufView[1]!, bufView[2]!, bufView[3]!) === '%PDF');
}

// <if expr="enable_pdf_save_to_drive">
function getChooserRequiredUrl(
    accountEmail: string, redirectUrl: string): string {
  const url = new URL(SAVE_TO_DRIVE_ACCOUNT_CHOOSER_URL);
  url.searchParams.set('Email', accountEmail);
  url.searchParams.set('faa', '1');
  url.searchParams.set('continue', redirectUrl);
  return url.href;
}

export function getSaveToDriveManageStorageUrl(
    accountEmail: string, accountIsManaged: boolean): string {
  const redirectUrl = accountIsManaged ?
      SAVE_TO_DRIVE_DASHER_MANAGE_STORAGE_URL :
      SAVE_TO_DRIVE_CONSUMER_MANAGE_STORAGE_URL;

  return getChooserRequiredUrl(accountEmail, redirectUrl);
}

export function getSaveToDriveOpenInDriveUrl(
    accountEmail: string, driveItemId: string): string {
  const url = new URL(SAVE_TO_DRIVE_DRIVE_URL);
  url.searchParams.set('action', 'locate');
  url.searchParams.set('id', driveItemId);
  return getChooserRequiredUrl(accountEmail, url.href);
}
// </if> enable_pdf_save_to_drive
