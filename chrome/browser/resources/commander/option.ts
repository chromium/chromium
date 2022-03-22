// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './icons.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {Option} from './types.js';

/**
 * Represents a substring of the option title, annotated with whether it's part
 * of a match or not.
 */
export type MatchSpan = {
  text: string,
  isMatch: boolean,
};

export class CommanderOptionElement extends PolymerElement {
  static get is() {
    return 'commander-option';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      model: Object,
    };
  }

  model: Option;

  /**
   * Splits this.model.title into a list of substrings, each marked with
   * whether they should be displayed as a match or not.
   */
  private computeMatchSpans_(): MatchSpan[] {
    const result: MatchSpan[] = [];
    let firstNonmatch = 0;
    for (const r of this.model.matchedRanges) {
      const [start, end] = r;
      if (start !== 0) {
        result.push({
          text: this.model.title.substring(firstNonmatch, start),
          isMatch: false
        });
      }
      result.push(
          {text: this.model.title.substring(start, end), isMatch: true});
      firstNonmatch = end;
    }
    if (firstNonmatch < this.model.title.length) {
      result.push(
          {text: this.model.title.substring(firstNonmatch), isMatch: false});
    }
    return result;
  }

  private getClassForMatch_(isMatch: boolean): string {
    return isMatch ? 'match' : '';
  }
}
customElements.define(CommanderOptionElement.is, CommanderOptionElement);
