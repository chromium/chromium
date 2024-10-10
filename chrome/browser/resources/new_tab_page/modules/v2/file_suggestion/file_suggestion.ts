// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {File} from '../../../file_suggestion.mojom-webui.js';

import {getCss} from './file_suggestion.css.js';
import {getHtml} from './file_suggestion.html.js';

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
      imageSourceBaseUrl: {type: String},
      moduleName: {type: String},
    };
  }

  files: File[] = [];
  imageSourceBaseUrl: string;
  moduleName: string;

  protected getImageSrc_(file: File): string {
    return this.imageSourceBaseUrl + file.mimeType;
  }

  protected onFileClick_(e: Event) {
    const clickFileEvent = new Event('usage', {composed: true, bubbles: true});
    this.dispatchEvent(clickFileEvent);
    const currentTarget = e.currentTarget as HTMLElement;
    const index = Number(currentTarget.dataset['index']);
    chrome.metricsPrivate.recordSmallCount(
        `NewTabPage.${this.moduleName}.FileClick`, index);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ntp-file-suggestion': FileSuggestionElement;
  }
}

customElements.define(FileSuggestionElement.is, FileSuggestionElement);
