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

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './search_engine_icon.html.js';
import type {SearchEngine} from './search_engines_browser_proxy.js';

export interface SettingsSearchEngineIconElement {
  $: {
    downloadedIcon: HTMLImageElement,
  };
}

export class SettingsSearchEngineIconElement extends PolymerElement {
  static get is() {
    return 'settings-search-engine-icon';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      engine: {
        type: Object,
        observer: 'onEngineChanged_',
      },

      showDownloadedIcon_: {
        type: Boolean,
        value: false,
      },
    };
  }

  declare engine: SearchEngine;
  private declare showDownloadedIcon_: boolean;
  private timeoutId_: number|null = null;

  private getIconUrl_(iconURL: string): string {
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
      newEngine: SearchEngine, oldEngine: SearchEngine|undefined) {
    if (oldEngine && newEngine.iconURL === oldEngine.iconURL) {
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

  private onDownloadedIconLoadSuccess_() {
    this.showDownloadedIcon_ = true;
    if (this.timeoutId_) {
      clearTimeout(this.timeoutId_);
      this.timeoutId_ = null;
    }
  }

  private onDownloadedIconLoadError_() {
    this.showDownloadedIcon_ = false;
  }

  private shouldShowDownloadedIcon_(): boolean {
    return this.showDownloadedIcon_ && !this.engine.iconPath &&
        !!this.engine.iconURL;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-search-engine-icon': SettingsSearchEngineIconElement;
  }
}

customElements.define(
    SettingsSearchEngineIconElement.is, SettingsSearchEngineIconElement);
