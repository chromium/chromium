// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_components/composebox/composebox_input.js';

import type {PageHandlerRemote} from '//resources/cr_components/composebox/composebox.mojom-webui.js';
import type {ComposeboxInputElement} from '//resources/cr_components/composebox/composebox_input.js';
import {ComposeboxEmbedderMixin} from '//resources/cr_components/composebox/composebox_mixin.js';
import {ComposeboxProxyImpl} from '//resources/cr_components/composebox/composebox_proxy.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PageHandlerRemote as SearchboxPageHandlerRemote, SearchContext} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';

import {getCss} from './omnibox_composebox.css.js';
import {getHtml} from './omnibox_composebox.html.js';

export interface OmniboxComposeboxElement {
  $: {
    composeboxInput: ComposeboxInputElement,
    composebox: HTMLElement,
  };
}

export class OmniboxComposeboxElement extends ComposeboxEmbedderMixin
(CrLitElement) {
  static get is() {
    return 'cr-omnibox-composebox';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      disableCaretColorAnimation: {
        type: Boolean,
        reflect: true,
      },
      entrypointName: {type: String, reflect: true},
    };
  }

  accessor disableCaretColorAnimation: boolean = false;
  accessor entrypointName: string = 'Omnibox';
  private pageHandler_: PageHandlerRemote;
  private searchboxHandler_: SearchboxPageHandlerRemote;

  constructor() {
    super();
    this.pageHandler_ = ComposeboxProxyImpl.getInstance().handler;
    this.searchboxHandler_ = ComposeboxProxyImpl.getInstance().searchboxHandler;
  }

  override getActiveElement(): Element|null {
    return this.shadowRoot?.activeElement || null;
  }

  override getInputElement(): ComposeboxInputElement {
    return this.$.composeboxInput;
  }

  override getPageHandler(): PageHandlerRemote {
    return this.pageHandler_;
  }

  override getSearchboxHandler(): SearchboxPageHandlerRemote {
    return this.searchboxHandler_;
  }

  // TODO(crbug.com/486707998): Remove once this is added to mixin.
  playGlowAnimation() {
    return;
  }

  // TODO(crbug.com/486707998): Implement when carousel is added.
  addSearchContext(context: SearchContext|null) {
    return context;
  }
}


declare global {
  interface HTMLElementTagNameMap {
    'cr-omnibox-composebox': OmniboxComposeboxElement;
  }
}

customElements.define(OmniboxComposeboxElement.is, OmniboxComposeboxElement);
