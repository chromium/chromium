// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-search-engine-icon' is the icon for a search engine in the settings
 * page. It prioritizes showing an existing favicon, then tries downloading an
 * image, and uses a generic favicon as final fallback.
 */
import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import '../site_favicon.js';

import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './search_engine_icon.css.js';
import {getHtml} from './search_engine_icon.html.js';
import type {SearchEngine} from './search_engines_browser_proxy.js';

export interface SettingsSearchEngineIconElement {
  $: {
    downloadedIcon: HTMLImageElement,
  };
}

export class SettingsSearchEngineIconElement extends CrLitElement {
  static get is() {
    return 'settings-search-engine-icon';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      engine: {type: Object},
      showDownloadedIcon_: {type: Boolean},
    };
  }

  accessor engine: SearchEngine|null = null;
  protected accessor showDownloadedIcon_: boolean = false;
  private timeoutId_: number|null = null;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('engine')) {
      const oldEngine = changedProperties.get('engine');
      this.onEngineChanged_(this.engine, oldEngine);
    }
  }

  protected getIconUrl_(): string {
    const iconURL = this.engine?.iconURL;
    if (!iconURL) {
      return '';
    }
    try {
      new URL(iconURL);
      return iconURL;
    } catch (e) {
      return '';
    }
  }

  private onEngineChanged_(
      newEngine: SearchEngine|null, oldEngine: SearchEngine|null|undefined) {
    if (oldEngine && newEngine?.iconURL === oldEngine.iconURL) {
      return;
    }
    this.showDownloadedIcon_ = false;
    if (this.timeoutId_) {
      clearTimeout(this.timeoutId_);
      this.timeoutId_ = null;
    }

    this.timeoutId_ = setTimeout(() => {
      if (!this.$.downloadedIcon.complete) {
        // Reset src to cancel ongoing request.
        this.$.downloadedIcon.src = '';
        this.showDownloadedIcon_ = false;
      }
      this.timeoutId_ = null;
    }, 1000);
  }

  protected onDownloadedIconLoad_() {
    this.showDownloadedIcon_ = true;
    if (this.timeoutId_) {
      clearTimeout(this.timeoutId_);
      this.timeoutId_ = null;
    }
  }

  protected onDownloadedIconError_() {
    this.showDownloadedIcon_ = false;
  }

  protected shouldShowDownloadedIcon_(): boolean {
    return this.showDownloadedIcon_ && !this.engine?.iconPath &&
        !!this.engine?.iconURL;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-search-engine-icon': SettingsSearchEngineIconElement;
  }
}

customElements.define(
    SettingsSearchEngineIconElement.is, SettingsSearchEngineIconElement);
