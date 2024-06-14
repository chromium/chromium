// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Modified for MV2.
 *
 * @fileoverview Definitions for chrome.brailleDisplayPrivate API
 * Generated from: chrome/common/extensions/api/braille_display_private.idl
 * run `tools/json_schema_compiler/compiler.py
 * chrome/common/extensions/api/braille_display_private.idl -g ts_definitions`
 * to regenerate.
 */

import {ChromeEvent} from '../../../../../../tools/typescript/definitions/chrome_event.js';

declare global {
  export namespace chrome {

    export namespace brailleDisplayPrivate {

      export enum KeyCommand {
        LINE_UP = 'line_up',
        LINE_DOWN = 'line_down',
        PAN_LEFT = 'pan_left',
        PAN_RIGHT = 'pan_right',
        TOP = 'top',
        BOTTOM = 'bottom',
        ROUTING = 'routing',
        SECONDARY_ROUTING = 'secondary_routing',
        DOTS = 'dots',
        CHORD = 'chord',
        STANDARD_KEY = 'standard_key',
      }

      export interface KeyEvent {
        command: KeyCommand;
        displayPosition?: number;
        brailleDots?: number;
        standardKeyCode?: string;
        standardKeyChar?: string;
        spaceKey?: boolean;
        altKey?: boolean;
        shiftKey?: boolean;
        ctrlKey?: boolean;
      }

      export interface DisplayState {
        available: boolean;
        textRowCount?: number;
        textColumnCount?: number;
        cellSize?: number;
      }

      type DisplayStateCallback = (state: DisplayState) => void;
      export function getDisplayState(callback: DisplayStateCallback): void;

      export function writeDots(
          cells: ArrayBuffer, columns: number, rows: number): void;

      export function updateBluetoothBrailleDisplayAddress(address: string):
          void;

      export const onDisplayStateChanged:
          ChromeEvent<(state: DisplayState) => void>;

      export const onKeyEvent: ChromeEvent<(event: KeyEvent) => void>;

    }
  }
}
