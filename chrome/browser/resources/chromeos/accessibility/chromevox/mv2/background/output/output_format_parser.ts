// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides a push parser for Output format rules.
 */
import {OutputFormatTree} from './output_format_tree.js';

type Annotation = any;

interface ParseOptions {
  annotation: Annotation[];
  isUnique: boolean;
}

/**
 * Implemented by objects that wish to observe tokens from parsing Output format
 * rules.
 */
export interface OutputFormatParserObserver {
  /**
   * Indicates the parse start of a new token.
   * @return True to skip to the next token.
   */
  onTokenStart(token: string): boolean | undefined;

  /**
   * Indicates a node attribute or special token (see output.js).
   * @return True to skip to the next token.
   */
  onNodeAttributeOrSpecialToken(
      token: string, tree: OutputFormatTree, options: ParseOptions):
      boolean | undefined;

  /**
   * Indicates a message token.
   * @return True to skip to the next token.
   */
  onMessageToken(
      token: string, tree: OutputFormatTree, options: ParseOptions):
      boolean | undefined;

  /**
   * Indicates a speech property token.
   * @return True to skip to the next token.
   */
  onSpeechPropertyToken(
      token: string, tree: OutputFormatTree, options: ParseOptions):
      boolean | undefined;

  /**
   * Indicates the parse end of a new token.
   * @return True to skip to the next token.
   */
  onTokenEnd(): boolean | undefined;
}

export class OutputFormatParser {
  private observer_: OutputFormatParserObserver;

  constructor(observer: OutputFormatParserObserver) {
    this.observer_ = observer;
  }

  /** Starts parsing the given output format. */
  parse(format: string | OutputFormatTree): void {
    const formatTrees: OutputFormatTree[] = OutputFormatTree.parseFormat(format);
    formatTrees.forEach((tree: OutputFormatTree) => {
      // Obtain the operator token.
      let token: string = tree.value;

      // Set suffix options.
      const options: ParseOptions = {annotation: [], isUnique: false};
      options.isUnique = token[token.length - 1] === '=';
      if (options.isUnique) {
        token = token.substring(0, token.length - 1);
      }

      // Process token based on prefix.
      const prefix = token[0];
      token = token.slice(1);

      if (this.observer_.onTokenStart(token)) {
        return;
      }

      // All possible tokens based on prefix.
      let skipToNextToken = false;
      // TODO(b/314203187): Not null asserted, check that this is correct.
      if (prefix === '$') {
        skipToNextToken =
            this.observer_.onNodeAttributeOrSpecialToken(token, tree, options)!;
      } else if (prefix === '@') {
        skipToNextToken = this.observer_.onMessageToken(token, tree, options)!;
      } else if (prefix === '!') {
        skipToNextToken =
            this.observer_.onSpeechPropertyToken(token, tree, options)!;
      }

      if (skipToNextToken) {
        return;
      }

      this.observer_.onTokenEnd();
    });
  }
}
