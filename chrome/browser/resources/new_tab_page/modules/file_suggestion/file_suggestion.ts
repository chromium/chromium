// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {File} from '../../file_suggestion.mojom-webui.js';
import {RecommendationType} from '../../file_suggestion.mojom-webui.js';
import {recordEnumeration, recordSmallCount} from '../../metrics_utils.js';

import {getCss} from './file_suggestion.css.js';
import {getHtml} from './file_suggestion.html.js';

/**
 * Actions the user can perform within a file suggestion module (Drive,
 * MicrosoftFiles). This enum must match the numbering for NTPFileAction in
 * histogram/enums.xml. These values are persisted to logs. Entries should not
 * be renumbered, removed or reused.
 */
// LINT.IfChange(FileAction)
export enum FileAction {
  FILE_SUGGESTION_CLICKED = 0,
  SEE_MORE_CLICKED = 1,
  MAX_VALUE = SEE_MORE_CLICKED,
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/new_tab_page/enums.xml:NTPFileAction)

export interface FileSuggestionElement {
  $: {
    files: HTMLElement,
  };
}

/**
 * Shared component for file modules, which serve as an inside look to recent
 * activity within a user's Google Drive or Microsoft Sharepoint.
 */
export class FileSuggestionElement extends CrLitElement {
  static get is() {
    return 'ntp-file-suggestion';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      files: {type: Array},
      moduleName: {type: String},
      seeMoreUrl: {type: String},
      seeMoreText: {type: String},
      seeMoreAriaLabel: {type: String},
    };
  }

  accessor files: File[] = [];
  accessor moduleName: string = '';
  accessor seeMoreUrl: string = '';
  accessor seeMoreText: string = '';
  accessor seeMoreAriaLabel: string = '';

  private recordFileAction_(action: FileAction) {
    recordEnumeration(
        `NewTabPage.${this.moduleName}.UserAction`, action,
        FileAction.MAX_VALUE + 1);
  }

  protected onFileClick_(e: Event) {
    const clickFileEvent = new Event('usage', {composed: true, bubbles: true});
    this.dispatchEvent(clickFileEvent);
    const currentTarget = e.currentTarget as HTMLElement;
    const index = Number(currentTarget.dataset['index']);
    recordSmallCount(`NewTabPage.${this.moduleName}.FileClick`, index);
    const file = this.files[index];
    assert(file);
    if (file.recommendationType !== null) {
      recordEnumeration(
          `NewTabPage.${this.moduleName}.RecommendationTypeClick`,
          file.recommendationType, RecommendationType.MAX_VALUE + 1);
    }
    this.recordFileAction_(FileAction.FILE_SUGGESTION_CLICKED);
  }

  protected onSeeMoreClick_() {
    this.dispatchEvent(new Event('usage', {composed: true, bubbles: true}));
    this.recordFileAction_(FileAction.SEE_MORE_CLICKED);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ntp-file-suggestion': FileSuggestionElement;
  }
}

customElements.define(FileSuggestionElement.is, FileSuggestionElement);
