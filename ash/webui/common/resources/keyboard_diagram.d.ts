// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {KeyboardKeyState} from './keyboard_key.js';

export enum MechanicalLayout {
  ANSI = 'ansi',
  ISO = 'iso',
  JIS = 'jis',
}

export enum PhysicalLayout {
  CHROME_OS = 'chrome-os',
  CHROME_OS_DELL_ENTERPRISE_WILCO = 'dell-enterprise-wilco',
  CHROME_OS_DELL_ENTERPRISE_DRALLION = 'dell-enterprise-drallion',
}

export enum TopRightKey {
  POWER = 'power',
  LOCK = 'lock',
  CONTROL_PANEL = 'control-panel',
}

interface TopRowKeyInterface {
  [index: string]: {icon?: string, ariaNameI18n?: string, text?: string};
}

export class KeyboardDiagramElement extends HTMLElement {
  topRightKey: TopRightKey;
  showNumberPad: boolean;
  setKeyState(evdevCode: number, state: KeyboardKeyState): void;
  setTopRowKeyState(topRowPosition: number, state: KeyboardKeyState): void;
  clearPressedKeys(): void;
  resetAllKeys(): void;
}

export const TopRowKey: TopRowKeyInterface;

declare global {
  interface HTMLElementTagNameMap {
    'keyboard-diagram': KeyboardDiagramElement;
  }
}
