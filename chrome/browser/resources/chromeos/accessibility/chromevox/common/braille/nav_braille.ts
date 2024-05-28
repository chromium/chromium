// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A simple container object for the brailling of a
 * navigation from one object to another.
 *
 */

/**
 * A class capturing the braille for navigation from one object to
 * another.
 */
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {SerializedSpannable, Spannable} from '../spannable.js';

/**
 * text The text of the object itself, including text from
 *     titles, labels, etc.
 * startIndex The beginning of a selection within text.
 * endIndex The end of a selection within text.
 */
interface KwArgs {
  endIndex?: number;
  startIndex?: number;
  text?: string | Spannable;
}

interface SerializedNavBraille {
  endIndex: number;
  startIndex: number;
  spannable: SerializedSpannable;
}

export class NavBraille {
  /** Text, annotated with DOM nodes. */
  text: Spannable;
  /** Selection start index. */
  startIndex: number;
  /** Selection end index. */
  endIndex: number;

  constructor(kwargs: KwArgs) {
    this.text = (kwargs.text instanceof Spannable) ? kwargs.text :
                                                     new Spannable(kwargs.text);

    this.startIndex =
        (kwargs.startIndex !== undefined) ? kwargs.startIndex : -1;

    this.endIndex =
        (kwargs.endIndex !== undefined) ? kwargs.endIndex : this.startIndex;
  }

  /**
   * Convenience for creating simple braille output.
   * @param text Text to represent in braille.
   * @return Braille output without a cursor.
   */
  static fromText(text: string | Spannable): NavBraille {
    return new NavBraille({text});
  }

  /**
   * Creates a NavBraille from its serialized json form as created
   * by toJson().
   * @param json the serialized json object.
   */
  static fromJson(json: SerializedNavBraille): NavBraille {
    if (typeof json.startIndex !== 'number' ||
        typeof json.endIndex !== 'number') {
      throw 'Invalid start or end index in serialized NavBraille: ' + json;
    }
    return new NavBraille({
      text: Spannable.fromJson(json.spannable),
      startIndex: json.startIndex,
      endIndex: json.endIndex,
    });
  }

  /** @return true if this braille description is empty. */
  isEmpty(): boolean {
    return this.text.length === 0;
  }

  /** @return A string representation of this object. */
  toString(): string {
    return 'NavBraille(text="' + this.text.toString() + '" ' +
        ' startIndex="' + this.startIndex + '" ' +
        ' endIndex="' + this.endIndex + '")';
  }

  /**
   * Returns a plain old data object with the same data.
   * Suitable for JSON encoding.
   */
  toJson(): SerializedNavBraille {
    return {
      spannable: this.text.toJson(),
      startIndex: this.startIndex,
      endIndex: this.endIndex,
    };
  }
}

TestImportManager.exportForTesting(NavBraille);
