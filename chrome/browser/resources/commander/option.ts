// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './icons.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {assertNotReached} from 'chrome://resources/js/assert_ts.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {Entity, Option} from './types.js';

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

  private computeIcon_(): string {
    switch (this.model.entity) {
      case Entity.COMMAND:
        return 'commander-icons:chrome';
      case Entity.BOOKMARK:
        return 'commander-icons:bookmark';
      case Entity.TAB:
        return 'commander-icons:tab';
      case Entity.WINDOW:
        return 'commander-icons:window';
      case Entity.GROUP:
        return 'commander-icons:group';
    }
    assertNotReached();
  }

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
