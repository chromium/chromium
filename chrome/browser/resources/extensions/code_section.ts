// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';
import './shared_vars.css.js';

import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './code_section.css.js';
import {getHtml} from './code_section.html.js';


function visibleLineCount(totalCount: number, oppositeCount: number): number {
  // We limit the number of lines shown for DOM performance.
  const MAX_VISIBLE_LINES = 1000;
  const max =
      Math.max(MAX_VISIBLE_LINES / 2, MAX_VISIBLE_LINES - oppositeCount);
  return Math.min(max, totalCount);
}

// TODO (rbpotter): Rename back to ExtensionsCodeSectionElement when .html.ts
// files are checked in.
const CodeSectionElementBase = I18nMixinLit(CrLitElement);

export class CodeSectionElement extends CodeSectionElementBase {
  static get is() {
    return 'extensions-code-section';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      code: {type: Object},
      isActive: {type: Boolean},

      /** Highlighted code. */
      highlighted_: {type: String},

      /** Code before the highlighted section. */
      before_: {type: String},

      /** Code after the highlighted section. */
      after_: {type: String},

      /** Description for the highlighted section. */
      highlightDescription_: {type: String},

      lineNumbers_: {type: String},
      truncatedBefore_: {type: Number},
      truncatedAfter_: {type: Number},

      /**
       * The string to display if no |code| is set (e.g. because we couldn't
       * load the relevant source file).
       */
      couldNotDisplayCode: {type: String},
    };
  }

  code: chrome.developerPrivate.RequestFileSourceResponse|null = null;
  isActive?: boolean;
  couldNotDisplayCode: string = '';
  protected highlighted_: string = '';
  protected before_: string = '';
  protected after_: string = '';
  protected highlightDescription_: string = '';
  protected lineNumbers_: string = '';
  protected truncatedBefore_: number = 0;
  protected truncatedAfter_: number = 0;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('code')) {
      this.onCodeChanged_();
    }
  }

  private async onCodeChanged_() {
    if (!this.code ||
        (!this.code.beforeHighlight && !this.code.highlight &&
         !this.code.afterHighlight)) {
      this.highlighted_ = '';
      this.highlightDescription_ = '';
      this.before_ = '';
      this.after_ = '';
      this.lineNumbers_ = '';
      return;
    }

    const before = this.code.beforeHighlight;
    const highlight = this.code.highlight;
    const after = this.code.afterHighlight;

    const linesBefore = before ? before.split('\n') : [];
    const linesAfter = after ? after.split('\n') : [];
    const visibleLineCountBefore =
        visibleLineCount(linesBefore.length, linesAfter.length);
    const visibleLineCountAfter =
        visibleLineCount(linesAfter.length, linesBefore.length);

    const visibleBefore =
        linesBefore.slice(linesBefore.length - visibleLineCountBefore)
            .join('\n');
    let visibleAfter = linesAfter.slice(0, visibleLineCountAfter).join('\n');
    // If the last character is a \n, force it to be rendered.
    if (visibleAfter.charAt(visibleAfter.length - 1) === '\n') {
      visibleAfter += ' ';
    }

    this.highlighted_ = highlight;
    this.highlightDescription_ = this.getAccessibilityHighlightDescription_(
        linesBefore.length, highlight.split('\n').length);
    this.before_ = visibleBefore;
    this.after_ = visibleAfter;
    this.truncatedBefore_ = linesBefore.length - visibleLineCountBefore;
    this.truncatedAfter_ = linesAfter.length - visibleLineCountAfter;

    const visibleCode = visibleBefore + highlight + visibleAfter;

    this.setLineNumbers_(
        this.truncatedBefore_ + 1,
        this.truncatedBefore_ + visibleCode.split('\n').length);

    // Happens asynchronously after the update completes
    await this.updateComplete;
    this.scrollToHighlight_(visibleLineCountBefore);
  }

  protected getLinesNotShownLabel_(
      lineCount: number, stringSingular: string,
      stringPluralTemplate: string): string {
    return lineCount === 1 ?
        stringSingular :
        loadTimeData.substituteString(stringPluralTemplate, lineCount);
  }

  private setLineNumbers_(start: number, end: number) {
    let lineNumbers = '';
    for (let i = start; i <= end; ++i) {
      lineNumbers += i + '\n';
    }

    this.lineNumbers_ = lineNumbers;
  }

  private scrollToHighlight_(linesBeforeHighlight: number) {
    const CSS_LINE_HEIGHT = 20;

    // Count how many pixels is above the highlighted code.
    const highlightTop = linesBeforeHighlight * CSS_LINE_HEIGHT;

    // Find the position to show the highlight roughly in the middle.
    const targetTop = highlightTop - this.clientHeight * 0.5;

    this.$['scroll-container'].scrollTo({top: targetTop});
  }

  private getAccessibilityHighlightDescription_(
      lineStart: number, numLines: number): string {
    if (numLines > 1) {
      return this.i18n(
          'accessibilityErrorMultiLine', lineStart.toString(),
          (lineStart + numLines - 1).toString());
    } else {
      return this.i18n('accessibilityErrorLine', lineStart.toString());
    }
  }

  protected shouldShowNoCode_(): boolean {
    return (this.isActive === undefined || this.isActive) && !this.highlighted_;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'extensions-code-section': CodeSectionElement;
  }
}


customElements.define(CodeSectionElement.is, CodeSectionElement);
