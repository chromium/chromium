// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

export const TopRowKey: TopRowKeyInterface;
