// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_components/composebox/composebox_dropdown.js';
import '//resources/cr_components/composebox/composebox_file_inputs.js';
import '//resources/cr_components/composebox/composebox_input.js';
import '//resources/cr_components/composebox/composebox_tool_chip.js';
import '//resources/cr_components/composebox/contextual_entrypoint_button.js';
import '//resources/cr_components/composebox/contextual_entrypoint_and_menu.js';
import '//resources/cr_components/composebox/composebox_submit.js';
import '//resources/cr_components/search/animated_glow.js';

import type {ComposeboxFile, TabUpload} from '//resources/cr_components/composebox/common.js';
import {getLoadTimeBoolean} from '//resources/cr_components/composebox/common.js';
import type {PageHandlerRemote} from '//resources/cr_components/composebox/composebox.mojom-webui.js';
import type {ComposeboxDropdownElement} from '//resources/cr_components/composebox/composebox_dropdown.js';
import type {ComposeboxInputElement} from '//resources/cr_components/composebox/composebox_input.js';
import {ComposeboxEmbedderMixin} from '//resources/cr_components/composebox/composebox_mixin.js';
import {ComposeboxProxyImpl} from '//resources/cr_components/composebox/composebox_proxy.js';
import type {ContextualEntrypointAndMenuElement} from '//resources/cr_components/composebox/contextual_entrypoint_and_menu.js';
import type {ContextualEntrypointButtonElement} from '//resources/cr_components/composebox/contextual_entrypoint_button.js';
import {GlowAnimationState} from '//resources/cr_components/search/constants.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {FileAttachment, PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote, SearchContext, TabAttachment} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {ToolMode} from '//resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';

import {getCss} from './everywhere_composebox.css.js';
import {getHtml} from './everywhere_composebox.html.js';

export interface EverywhereComposeboxElement {
  $: {
    composeboxInput: ComposeboxInputElement,
    composebox: HTMLElement,
    matches: ComposeboxDropdownElement,
  };
}

export class EverywhereComposeboxElement extends ComposeboxEmbedderMixin
(CrLitElement) {
  static get is() {
    return 'everywhere-composebox';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      applyContextButtonBackground: {
        reflect: true,
        type: Boolean,
      },
      entrypointName: {type: String, reflect: true},
      disableComposeboxAnimation: {type: Boolean},
    };
  }

  accessor entrypointName: string = 'Omnibox';
  accessor disableComposeboxAnimation: boolean = false;
  accessor applyContextButtonBackground: boolean = false;
  private webuiOmniboxSimplificationEnabled_: boolean =
      getLoadTimeBoolean('webuiOmniboxSimplificationEnabled', false);
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

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('inputState')) {
      const inToolMode = this.inputState?.activeTool !== ToolMode.kUnspecified;
      this.applyContextButtonBackground =
          this.webuiOmniboxSimplificationEnabled_ && !inToolMode;
    }
  }

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);
    this.focusInput();
  }

  override async addTabContextHandleCallback(
      _tabUpload: TabUpload, _replaceAutoActiveTabToken: boolean = false,
      _onBeforeUpdateFiles?: (attachment: ComposeboxFile) =>
          void): Promise<ComposeboxFile|null> {
    // Note: Copied from omnibox_composebox.ts. May need to be implemented
    // fully if adding file carousel.
    // For now, satisfy contract to avoid assertNotReached crashes on state
    // updates.
    return Promise.resolve(null);
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

  override getContextEntrypointElement(): ContextualEntrypointButtonElement|
      ContextualEntrypointAndMenuElement|null {
    return this.shadowRoot?.querySelector<ContextualEntrypointAndMenuElement>(
               '#contextEntrypoint') ||
        null;
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

  // Note: Copied from omnibox_composebox.ts. May need implementation when
  // carousel is added.
  private addFileFromAttachment_(fileAttachment: FileAttachment) {
    return fileAttachment;
  }

  // Note: Copied from omnibox_composebox.ts. May need implementation when
  // carousel is added.
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

  override selectFirstMatch() {
    if (this.result?.matches && this.result.matches.length > 0 &&
        this.result.matches[0]?.allowedToBeDefaultMatch &&
        this.selectedMatchIndex !== -1) {
      this.getDropdownElement().selectFirst();
    }
  }

  override submitQuery() {
    super.submitQuery();
  }

  override hasValidQuery(): boolean {
    // If there is at least one file that supports unimodal search, query is
    // valid.
    if (this.files.size > 0 &&
        Array.from(this.files.values()).some(file => file.supportsUnimodal)) {
      return true;
    }

    // If an autocomplete match is selected, it's a valid query.
    if (this.selectedMatchIndex >= 0 && !!this.result) {
      return true;
    }

    if (this.input.trim().length > 0) {
      return true;
    }

    return false;
  }

  playGlowAnimation() {
    this.animationState = GlowAnimationState.NONE;
    requestAnimationFrame(() => {
      this.animationState = GlowAnimationState.EXPANDING;
    });
  }
}


declare global {
  interface HTMLElementTagNameMap {
    'everywhere-composebox': EverywhereComposeboxElement;
  }
}

customElements.define(
    EverywhereComposeboxElement.is, EverywhereComposeboxElement);
