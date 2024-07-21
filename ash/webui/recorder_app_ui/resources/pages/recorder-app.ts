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

function getBoolean(search: URLSearchParams, key: string): boolean {
  return search.get(key) === 'true';
}

/**
 * Root route of Recorder App.
 */
export class RecorderApp extends ReactiveLitElement {
  static override styles = css`
    :host {
      display: block;
      height: 100%;
      width: 100%;
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
    //
    // We use hash based client side navigation, to avoid the following issue
    // for modern path based client side navigation in our use case:
    // * recorder_app_ui.cc needs to have all the paths that it should handle.
    // * When serving bundled output via cra.py bundle, many static hosting
    //   server (like x20) doesn't support path rewrite and doesn't work well
    //   with client side navigation.
    // * The route below needs to handle when the bundled output is hosted on a
    //   subpath.
    //
    // TODO(pihsun): Since changing hash won't trigger page refresh, we
    // probably can simplify some of the logic in core/state/route.ts.
    const routeInHash = new URL(
      currentRoute.value.hash.slice(1),
      // Note that the origin part is not used and we only use the path and
      // search, but URL constructor requires a base URL if the first argument
      // is just a path.
      document.location.origin,
    );
    const path = routeInHash.pathname;
    const search = new URLSearchParams(routeInHash.search);

    if (path === '/') {
      return html`<main-page></main-page>`;
    }
    if (path === '/playback') {
      const id = search.get('id');
      return html`<playback-page .recordingId=${id}></playback-page>`;
    }
    if (path === '/record') {
      const includeSystemAudio = getBoolean(search, 'includeSystemAudio');
      const micId = search.get('micId');
      return html`<record-page
        .includeSystemAudio=${includeSystemAudio}
        .micId=${micId}
      >
      </record-page>`;
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
