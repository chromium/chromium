// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {getFaviconForPageURL} from 'chrome://resources/js/icon.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './favicon_group.css.js';
import {getHtml} from './favicon_group.html.js';

const MAX_DISPLAY_COUNT = 3;

export class ContextualTasksFaviconGroupElement extends CrLitElement {
  static get is() {
    return 'contextual-tasks-favicon-group';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      urls: {type: Array},
      visibleUrls_: {type: Array},
      remainingCount_: {type: Number},
    };
  }

  accessor urls: string[] = [];
  protected accessor visibleUrls_: string[] = [];
  protected accessor remainingCount_: number = 0;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    if (changedProperties.has('urls')) {
      this.onUrlsChanged_();
    }
  }

  private onUrlsChanged_() {
    const numToDisplay = Math.min(this.urls.length, MAX_DISPLAY_COUNT);
    this.visibleUrls_ = this.urls.slice(0, numToDisplay);
    this.remainingCount_ = this.urls.length - numToDisplay;
  }

  protected getFaviconUrl_(url: string): string {
    return getFaviconForPageURL(url, false);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'contextual-tasks-favicon-group': ContextualTasksFaviconGroupElement;
  }
}

customElements.define(
    ContextualTasksFaviconGroupElement.is, ContextualTasksFaviconGroupElement);
