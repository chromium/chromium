// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/composebox/composebox_favicon_group.js';

import {ExtensionBrowserProxyImpl} from 'chrome://contextual-tasks/contextual_tasks_browser_proxy.js';
import type {ComposeboxFaviconGroupElement} from 'chrome://resources/cr_components/composebox/composebox_favicon_group.js';
import {TrackedElementManager} from 'chrome://resources/js/tracked_element/tracked_element_manager.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {CSSResultGroup, PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {TabInfo} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';

import {getCss} from './context_library.css.js';
import {getHtml} from './context_library.html.js';

export interface ContextLibraryElement {
  $: {
    faviconGroup: ComposeboxFaviconGroupElement,
  };
}

export class ContextLibraryElement extends CrLitElement {
  static get is() {
    return 'context-library';
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
      darkMode: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  accessor tabs: TabInfo[] = [];
  accessor submittedTabIds: Set<number> = new Set();
  accessor darkMode: boolean = false;

  private isTrackingFaviconGroup_: boolean = false;
  private resizeObserver_: ResizeObserver|null = null;
  private listenerIds_: number[] = [];

  override connectedCallback() {
    super.connectedCallback();
    this.darkMode = this.isDarkModeEnabled_();
    this.resizeObserver_ = new ResizeObserver(() => {
      window.requestResize?.();
    });
    this.resizeObserver_.observe(this);

    const callbackRouter =
        ExtensionBrowserProxyImpl.getInstance().callbackRouter;
    this.listenerIds_.push(callbackRouter.onTabContextUpdated.addListener(
        (tabs: TabInfo[], submittedTabIds: number[]) => {
          this.tabs = tabs;
          this.submittedTabIds = new Set(submittedTabIds);
        }));
    this.updateFaviconTracking_();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    if (this.isTrackingFaviconGroup_) {
      this.isTrackingFaviconGroup_ = false;
      TrackedElementManager.getInstance().stopTracking(this.$.faviconGroup);
    }
    this.resizeObserver_?.disconnect();
    this.resizeObserver_ = null;
    const callbackRouter =
        ExtensionBrowserProxyImpl.getInstance().callbackRouter;
    for (const id of this.listenerIds_) {
      callbackRouter.removeListener(id);
    }
    this.listenerIds_ = [];
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('tabs')) {
      this.updateFaviconTracking_();
    }
  }

  private updateFaviconTracking_() {
    const hasTabs = (this.tabs?.length ?? 0) > 0;
    if (hasTabs && !this.isTrackingFaviconGroup_) {
      this.isTrackingFaviconGroup_ = true;
      TrackedElementManager.getInstance().startTracking(
          this.$.faviconGroup, 'kContextualTasksFaviconsElementId');
    } else if (!hasTabs && this.isTrackingFaviconGroup_) {
      this.isTrackingFaviconGroup_ = false;
      TrackedElementManager.getInstance().stopTracking(this.$.faviconGroup);
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

  protected isDarkModeEnabled_(): boolean {
    const params = new URLSearchParams(window.location.search);
    return params.get('cs') === '1';
  }
}

declare global {
  interface Window {
    requestResize?: () => void;
  }

  interface HTMLElementTagNameMap {
    'context-library': ContextLibraryElement;
  }
}

customElements.define(ContextLibraryElement.is, ContextLibraryElement);
