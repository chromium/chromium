// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_components/composebox/composebox_dropdown.js';
import '//resources/cr_components/composebox/composebox_file_inputs.js';
import '//resources/cr_components/composebox/composebox_submit.js';
import '//resources/cr_components/composebox/contextual_entrypoint_and_menu.js';
import '//resources/cr_components/composebox/composebox_input.js';
import '//resources/cr_components/composebox/error_scrim.js';
import '//resources/cr_components/composebox/file_carousel.js';
import '//resources/cr_components/composebox/composebox_tool_chip.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_components/composebox/composebox_voice_search.js';
import '//resources/cr_components/search/animated_glow.js';
import '//resources/cr_components/localized_link/localized_link.js';

import {getLoadTimeBoolean} from '//resources/cr_components/composebox/common.js';
import type {ComposeboxFile} from '//resources/cr_components/composebox/common.js';
import type {PageHandlerRemote} from '//resources/cr_components/composebox/composebox.mojom-webui.js';
import type {ComposeboxDropdownElement} from '//resources/cr_components/composebox/composebox_dropdown.js';
import type {ComposeboxFileInputsElement} from '//resources/cr_components/composebox/composebox_file_inputs.js';
import type {ComposeboxInputElement} from '//resources/cr_components/composebox/composebox_input.js';
import {ComposeboxEmbedderMixin} from '//resources/cr_components/composebox/composebox_mixin.js';
import type {ComposeboxEmbedderMixinInterface} from '//resources/cr_components/composebox/composebox_mixin.js';
import {ComposeboxProxyImpl} from '//resources/cr_components/composebox/composebox_proxy.js';
import type {ContextualEntrypointAndMenuElement} from '//resources/cr_components/composebox/contextual_entrypoint_and_menu.js';
import type {ErrorScrimElement} from '//resources/cr_components/composebox/error_scrim.js';
import type {ComposeboxFileCarouselElement} from '//resources/cr_components/composebox/file_carousel.js';
import {GlowAnimationState} from '//resources/cr_components/search/constants.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {UnguessableToken} from '//resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';

import {getCss} from './ntp_composebox.css.js';
import {getHtml} from './ntp_composebox.html.js';

export interface NtpComposeboxElement extends ComposeboxEmbedderMixinInterface {
  $: {
    composeboxInput: ComposeboxInputElement,
    composebox: HTMLElement,
    matches: ComposeboxDropdownElement,
    fileInputs: ComposeboxFileInputsElement,
    carousel: ComposeboxFileCarouselElement,
    errorScrim: ErrorScrimElement,
  };
}

export class NtpComposeboxElement extends ComposeboxEmbedderMixin
(CrLitElement) {
  static get is() {
    return 'ntp-composebox';
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
      /*
      `expanding_` property is used in composebox.css styles. It is added
      so that the imported styles work well. Remove this property once each
      embedder has its own styles.
      */
      expanding_: {
        reflect: true,
        type: Boolean,
      },
      shouldRemainFolded_: {
        reflect: true,
        type: Boolean,
      },
      isDark: {
        reflect: true,
        type: Boolean,
      },
    };
  }

  accessor isDark: boolean = false;
  accessor entrypointName: string = 'Realbox';
  private searchboxCallbackRouter_: SearchboxPageCallbackRouter;
  private pageHandler_: PageHandlerRemote;
  private searchboxHandler_: SearchboxPageHandlerRemote;
  private eventTracker_: EventTracker = new EventTracker();
  protected accessor expanding_: boolean = true;
  protected accessor shouldRemainFolded_: boolean = true;

  override get keepMenuOpenOnTabSelect(): boolean {
    return getLoadTimeBoolean('keepMenuOpenOnTabSelectForRealbox', false);
  }

  override getPageHandler(): PageHandlerRemote {
    return this.pageHandler_;
  }

  override getSearchboxHandler(): SearchboxPageHandlerRemote {
    return this.searchboxHandler_;
  }

  override getSearchboxCallbackRouter(): SearchboxPageCallbackRouter {
    return this.searchboxCallbackRouter_;
  }

  override getActiveElement(): Element|null {
    return this.shadowRoot?.activeElement || null;
  }

  override getInputElement(): ComposeboxInputElement {
    return this.$.composeboxInput;
  }

  override getDropdownElement(): ComposeboxDropdownElement {
    return this.$.matches;
  }

  override getContextEntrypointElement(): ContextualEntrypointAndMenuElement|
      null {
    return this.shadowRoot?.querySelector<ContextualEntrypointAndMenuElement>(
               '#contextEntrypoint') ||
        null;
  }

  constructor() {
    super();
    this.pageHandler_ = ComposeboxProxyImpl.getInstance().handler;
    this.searchboxCallbackRouter_ =
        ComposeboxProxyImpl.getInstance().searchboxCallbackRouter;
    this.searchboxHandler_ = ComposeboxProxyImpl.getInstance().searchboxHandler;
  }

  override connectedCallback() {
    super.connectedCallback();
    this.animationState = GlowAnimationState.EXPANDING;
    this.focusInput();
    this.refreshTabSuggestions(/*forceRefresh=*/ true);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    this.shouldRemainFolded_ = this.computeShouldRemainFolded();
  }

  private computeShouldRemainFolded(): boolean {
    if (this.errorMessage) {
      return false;
    }
    if ((this.files?.size ?? 0) > 0) {
      return false;
    }
    if (this.inToolMode) {
      return false;
    }
    if ((this.result?.matches?.length ?? 0) > 0) {
      return false;
    }
    return true;
  }

  override shouldShowDivider(): boolean {
    const hasNonTabFiles = Array.from(this.files.values()).some(f => !f.url);
    if (this.hasTabs() && !hasNonTabFiles) {
      return this.showDropdown;
    }
    return super.shouldShowDivider();
  }

  override deleteFile(uuidToDelete: UnguessableToken, fromUserAction?: boolean):
      ComposeboxFile|null {
    const file = super.deleteFile(uuidToDelete, fromUserAction);
    if (file) {
      this.queryAutocomplete(/* clearMatches= */ true);
    }
    return file;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ntp-composebox': NtpComposeboxElement;
  }
}

customElements.define(NtpComposeboxElement.is, NtpComposeboxElement);
