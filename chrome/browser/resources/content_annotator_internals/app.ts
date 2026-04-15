// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {Value} from '//resources/mojo/mojo/public/mojom/base/values.mojom-webui.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {BrowserProxyImpl} from './browser_proxy.js';
import type {BrowserProxy} from './browser_proxy.js';

export interface AnnotationEntry {
  visit_id: string;
  navigation_timestamp: string;
  url: string;
  title: string;
  tab_id?: number;
  content_annotation: any;
  classifier_results: any;
}

export class ContentAnnotatorInternalsAppElement extends CrLitElement {
  static get is() {
    return 'content-annotator-internals-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      logContent_: {type: Array},
      errorMessage_: {type: String},
    };
  }

  protected accessor logContent_: AnnotationEntry[] = [];
  protected accessor errorMessage_: string = '';
  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();
    this.loadLogContent_();
  }

  private async loadLogContent_() {
    this.errorMessage_ = '';
    try {
      const {content} = await this.browserProxy_.handler.getAnnotatedContent();
      this.logContent_ = this.flattenValue_(content) || [];
    } catch (e) {
      this.errorMessage_ = 'Error: could not get content annotations.';
      this.logContent_ = [];
    }
  }

  protected async onClearCacheClick_() {
    this.errorMessage_ = '';
    try {
      const {success} =
          await this.browserProxy_.handler.clearAnnotatedContent();
      if (success) {
        this.loadLogContent_();
      } else {
        this.errorMessage_ = 'Error: could not clear content annotations cache.';
      }
    } catch (e) {
      this.errorMessage_ = 'Error: could not clear content annotations cache.';
    }
  }

  private flattenValue_(value: Value): any {
    if (!value) {
      return null;
    }

    if (value.boolValue !== undefined) {
      return value.boolValue;
    }
    if (value.intValue !== undefined) {
      return value.intValue;
    }
    if (value.doubleValue !== undefined) {
      return value.doubleValue;
    }
    if (value.stringValue !== undefined) {
      return value.stringValue;
    }
    if (value.listValue !== undefined) {
      return value.listValue.storage.map(v => this.flattenValue_(v));
    }
    if (value.dictionaryValue !== undefined) {
      const flattened: {[key: string]: any} = {};
      for (const [k, v] of Object.entries(value.dictionaryValue.storage)) {
        flattened[k] = this.flattenValue_(v);
      }
      return flattened;
    }
    return null;
  }

  protected formatJson_(data: any): string {
    return JSON.stringify(data, null, 2);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'content-annotator-internals-app': ContentAnnotatorInternalsAppElement;
  }
}

customElements.define(
    ContentAnnotatorInternalsAppElement.is,
    ContentAnnotatorInternalsAppElement);
