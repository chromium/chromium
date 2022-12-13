// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: TypeScript UIs should generally use load_time_data.ts directly, and
// not rely on these definitions. These are checked in so that UIs in
// transition can keep relying on load_time_data.m.js.

// eslint-disable-next-line no-var
export var loadTimeData: LoadTimeData;
declare class LoadTimeData {
  set data(arg: any);
  valueExists(id: string): boolean;
  getValue(id: string): any;
  getString(id: string): string;
  getStringF(id: string, ...args: Array<string|number>): string;
  substituteString(label: string, ...args: Array<string|number>): string;
  getSubstitutedStringPieces(label: string, ...args: Array<string|number>):
      Array<{value: string, arg: (null|string)}>;
  getBoolean(id: string): boolean;
  getInteger(id: string): number;
  overrideValues(replacements: any): void;
  resetForTesting(newData?: any|null): void;
  isInitialized(): boolean;
}
