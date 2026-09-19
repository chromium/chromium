// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/composebox/composebox_favicon_group.js';

import type {ComposeboxFaviconGroupElement} from 'chrome://resources/cr_components/composebox/composebox_favicon_group.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {CSSResultGroup} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {TabInfo} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';

import {getCss} from './favicons_app.css.js';
import {getHtml} from './favicons_app.html.js';

export interface FaviconsAppElement {
  $: {
    faviconGroup: ComposeboxFaviconGroupElement,
  };
}

export class FaviconsAppElement extends CrLitElement {
  static get is() {
    return 'favicons-app';
  }

  static override get styles(): CSSResultGroup {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      tabs: {type: Array},
      submittedTabIds: {type: Object},
    };
  }

  // TODO(crbug.com/): Populate from the browser once the extension page
  // handler exposes tab context. `ContextualTasksExtensionHandler::
  // GetRecentTabs()` is currently stubbed to return an empty list, so this
  // stays empty until a real data source is wired up.
  accessor tabs: TabInfo[] = [];
  accessor submittedTabIds: Set<number> = new Set();

  private resizeObserver_: ResizeObserver|null = null;
  private messageListener_: ((event: MessageEvent) => void)|null = null;

  override connectedCallback() {
    super.connectedCallback();
    this.resizeObserver_ = new ResizeObserver(() => {
      window.requestResize?.();
    });
    this.resizeObserver_.observe(this);

    this.messageListener_ = (event: MessageEvent) => {
      if (event.data?.type === 'SET_TABS' && Array.isArray(event.data.tabs)) {
        this.tabs = event.data.tabs;
      }
    };
    window.addEventListener('message', this.messageListener_);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.resizeObserver_?.disconnect();
    this.resizeObserver_ = null;
    if (this.messageListener_) {
      window.removeEventListener('message', this.messageListener_);
      this.messageListener_ = null;
    }
  }

  // <composebox-favicon-group> fires this to request a live favicon for a tab
  // that is still loading. Handled here so the event does not escape the
  // component; invoking `onTabLoaded` swaps in the higher fidelity favicon.
  protected onWaitForTabLoad_(
      _e: CustomEvent<{tabId: number, onTabLoaded: (url?: string) => void}>) {
    // TODO(crbug.com/): Forward to the browser and call `onTabLoaded` with the
    // resolved favicon data URL. Until then the group falls back to
    // getFaviconForPageURL().
  }
}

declare global {
  interface Window {
    requestResize?: () => void;
  }

  interface HTMLElementTagNameMap {
    'favicons-app': FaviconsAppElement;
  }
}

customElements.define(FaviconsAppElement.is, FaviconsAppElement);
