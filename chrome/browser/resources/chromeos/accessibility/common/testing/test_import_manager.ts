// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

type Importable = Function|[name: string, value: any];

/**
 * Class to manage imports for tests within the js2gtest framework. The
 * framework does not allow for static import statements, and Service Workers
 * (required for manifest v3) do not allow dynamic import statements. Therefore
 * this custom solution is needed.
 */
export class TestImportManager {
  private static exports_: Importable[] = [];

  /**
   * Functions and classes are passed directly as arguments.
   * Other values are passed as a tuple, i.e. [name, value].
   */
  static exportForTesting(...exports: Importable[]): void {
    TestImportManager.exports_ = TestImportManager.exports_.concat(exports);
  }

  static importForTesting(): void {
    for (const exportValue of TestImportManager.exports_) {
      if (typeof exportValue === 'function') {
        globalThis.exports[exportValue.name as string] = exportValue;
      } else if (exportValue instanceof Array) {
        globalThis.exports[exportValue[0] as string] = exportValue[1];
      } else {
        console.warn('invalid test import:', exportValue);
      }
    }
  }
}

declare global {
  var exports: {[exportName: string]: any};
  var TestImportManager: Function;
}

globalThis.exports = {};
globalThis.TestImportManager = TestImportManager;
