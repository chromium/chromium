// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Defines a Braille interface.
 * All Braille engines in ChromeVox conform to this interface.
 */
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {BrailleDisplayState} from '../../common/braille/braille_key_types.js';
import {NavBraille} from '../../common/braille/nav_braille.js';

// TODO(anastasi): Convert to an interface once dependencies are converted to
//     TypeScript.
export abstract class BrailleInterface {
  /**
   * Sends the given params to the Braille display for output.
   * @param params Parameters to send to the platform braille service.
   */
  abstract write(params: NavBraille): void;

  /**
   * Takes an image in the form of a data url and outputs it to a Braille
   * display.
   * @param imageDataUrl The image to output, in the form of a dataUrl.
   */
  abstract writeRawImage(imageDataUrl: string): void;

  /**
   * Freeze whatever is on the braille display until the next call to thaw().
   */
  abstract freeze(): void;

  /** Un-freeze the braille display so that it can be written to again. */
  abstract thaw(): void;

  /** @return The current display state. */
  abstract getDisplayState(): BrailleDisplayState;

  /** Requests the braille display pan left. */
  abstract panLeft(): void;

  /** Requests the braille display pan right. */
  abstract panRight(): void;

  /**
   * Moves the cursor to the given braille position.
   * @param braillePosition The 0-based position relative to the start of the
   *     currently displayed text. The position is given in braille cells, not
   *     text cells.
   */
  abstract route(braillePosition: number | undefined): void;

  /**
   * Translate braille cells into text.
   * @param cells Cells to be translated.
   */
  abstract backTranslate(cells: ArrayBuffer): Promise<string|null>;
}

TestImportManager.exportForTesting(BrailleInterface);
