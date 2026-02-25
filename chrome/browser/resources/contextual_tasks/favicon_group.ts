// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';

import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {getFaviconForPageURL} from 'chrome://resources/js/icon.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ContextInfo} from './contextual_tasks.mojom-webui.js';
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
      contextInfos: {type: Array},
      visibleItems_: {type: Array},
      remainingCount_: {type: Number},
    };
  }

  accessor contextInfos: ContextInfo[] = [];
  protected accessor visibleItems_: ContextInfo[] = [];
  protected accessor remainingCount_: number = 0;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    if (changedProperties.has('contextInfos')) {
      this.onContextInfosChanged_();
    }
  }

  private onContextInfosChanged_() {
    this.visibleItems_ = this.contextInfos.slice(0, MAX_DISPLAY_COUNT);
    this.remainingCount_ = this.contextInfos.length - this.visibleItems_.length;
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
