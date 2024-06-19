// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './dev-page.js';
import './main-page.js';
import './playback-page.js';
import './record-page.js';

import {css, html, nothing} from 'chrome://resources/mwc/lit/index.js';

import {ReactiveLitElement} from '../core/reactive/lit.js';
import {currentRoute} from '../core/state/route.js';

/**
 * Root route of Recorder App.
 */
export class RecorderApp extends ReactiveLitElement {
  static override styles = css`
    :host {
      display: block;
      width: 100%;
      height: 100%;
    }
  `;

  private render404() {
    return 'Not found';
  }

  override render(): RenderResult {
    if (currentRoute.value === null) {
      return nothing;
    }

    // TODO(shik): Make page routes type-safe, so there is no missing or
    // wrongly typed search params when calling navigateTo().
    const path = currentRoute.value.pathname;
    const search = new URLSearchParams(currentRoute.value.search);
    if (path === '/' || path === '/static/index.html') {
      return html`<main-page></main-page>`;
    }
    if (path === '/playback') {
      const id = search.get('id');
      return html`<playback-page .recordingId=${id}></playback-page>`;
    }
    if (path === '/record') {
      const audioSource = search.get('audioSource');
      return html`<record-page .audioSource=${audioSource}></record-page>`;
    }
    if (path === '/dev') {
      return html`<dev-page></dev-page>`;
    }

    return this.render404();
  }
}

window.customElements.define('recorder-app', RecorderApp);

declare global {
  interface HTMLElementTagNameMap {
    'recorder-app': RecorderApp;
  }
}
