// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_components/composebox/composebox_dropdown.js';
import '//resources/cr_components/composebox/composebox_file_inputs.js';
import '//resources/cr_components/composebox/composebox_input.js';
import '//resources/cr_components/composebox/composebox_tool_chip.js';
import '//resources/cr_components/composebox/contextual_entrypoint_button.js';

import type {TabUpload} from '//resources/cr_components/composebox/common.js';
import type {PageHandlerRemote} from '//resources/cr_components/composebox/composebox.mojom-webui.js';
import type {ComposeboxDropdownElement} from '//resources/cr_components/composebox/composebox_dropdown.js';
import type {ComposeboxInputElement} from '//resources/cr_components/composebox/composebox_input.js';
import {ComposeboxEmbedderMixin} from '//resources/cr_components/composebox/composebox_mixin.js';
import {ComposeboxProxyImpl} from '//resources/cr_components/composebox/composebox_proxy.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {FileAttachment, PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote, SearchContext, TabAttachment} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';

import {getCss} from './omnibox_composebox.css.js';
import {getHtml} from './omnibox_composebox.html.js';

export interface OmniboxComposeboxElement {
  $: {
    composeboxInput: ComposeboxInputElement,
    composebox: HTMLElement,
    matches: ComposeboxDropdownElement,
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
      entrypointName: {type: String, reflect: true},
    };
  }

  accessor entrypointName: string = 'Omnibox';
  private pageHandler_: PageHandlerRemote;
  private searchboxCallbackRouter_: SearchboxPageCallbackRouter;
  private searchboxHandler_: SearchboxPageHandlerRemote;

  constructor() {
    super();
    this.pageHandler_ = ComposeboxProxyImpl.getInstance().handler;
    this.searchboxCallbackRouter_ =
        ComposeboxProxyImpl.getInstance().searchboxCallbackRouter;
    this.searchboxHandler_ = ComposeboxProxyImpl.getInstance().searchboxHandler;
  }

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);
    this.focusInput();
  }

  override async addTabContextHandleCallback(
      _tabUpload: TabUpload, _replaceAutoActiveTabToken: boolean = false) {
    // TODO(crbug.com/508287630): Implement fully when adding file carousel.
    // For now, satisfy contract to avoid assertNotReached crashes on state
    // updates.
    return Promise.resolve();
  }

  override getActiveElement(): Element|null {
    return this.shadowRoot?.activeElement || null;
  }

  override getDropdownElement(): ComposeboxDropdownElement {
    return this.$.matches;
  }

  override getInputElement(): ComposeboxInputElement {
    return this.$.composeboxInput;
  }

  override getPageHandler(): PageHandlerRemote {
    return this.pageHandler_;
  }

  override getSearchboxCallbackRouter(): SearchboxPageCallbackRouter {
    return this.searchboxCallbackRouter_;
  }

  override getSearchboxHandler(): SearchboxPageHandlerRemote {
    return this.searchboxHandler_;
  }

  override getContextEntrypointElement(): HTMLElement|null {
    return this.shadowRoot?.querySelector('#contextEntrypoint') || null;
  }

  addSearchContext(context: SearchContext|null) {
    if (context) {
      if (context.input.length > 0) {
        this.input = context.input;
      }
      for (const attachment of context.attachments) {
        if (attachment.fileAttachment) {
          this.addFileFromAttachment_(attachment.fileAttachment);
        } else if (attachment.tabAttachment) {
          this.addTabFromAttachment_(attachment.tabAttachment);
        }
      }
    }

    // Query for ZPS even if there's no context.
    if (this.showZps) {
      this.queryAutocomplete(/* clearMatches= */ false);
    }
  }

  // TODO(crbug.com/508287630): Implement when carousel is added.
  private addFileFromAttachment_(fileAttachment: FileAttachment) {
    return fileAttachment;
  }

  // TODO(crbug.com/508287630): Implement when carousel is added.
  private addTabFromAttachment_(tabAttachment: TabAttachment) {
    return tabAttachment;
  }

  override shouldShowDivider(): boolean {
    if (this.searchboxLayoutMode === 'TallBottomContext' &&
        !this.showFileCarousel) {
      return false;
    }

    return super.shouldShowDivider();
  }

  // TODO(crbug.com/486707998): Remove once this is added to mixin.
  playGlowAnimation() {
    return;
  }
}


declare global {
  interface HTMLElementTagNameMap {
    'cr-omnibox-composebox': OmniboxComposeboxElement;
  }
}

customElements.define(OmniboxComposeboxElement.is, OmniboxComposeboxElement);
