// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BackTranslateCallback, BrailleTranslator, TranslateCallback} from './braille_translator.js';

export class TenjiTranslator implements BrailleTranslator {
  init(): Promise<void> {
    // TODO(crbug.com/500394286): Implement this.
    return Promise.resolve();
  }

  translate(
      _text: string,
      // Tenji library doesn't support form type maps, so this parameter is
      // unused
      _formTypeMap: number[]|number, callback: TranslateCallback): void {
    // TODO(crbug.com/500394286): Implement this.
    callback(new ArrayBuffer(0), [], []);
  }

  backTranslate(_cells: ArrayBuffer, callback: BackTranslateCallback): void {
    // TODO(crbug.com/500394286): Implement this.
    callback(null);
  }
}
