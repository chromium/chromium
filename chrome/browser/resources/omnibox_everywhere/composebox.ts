// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_components/composebox/composebox_dropdown.js';
import '//resources/cr_components/composebox/composebox_file_inputs.js';
import '//resources/cr_components/composebox/composebox_input.js';
import '//resources/cr_components/composebox/composebox_tool_chip.js';
import '//resources/cr_components/composebox/contextual_entrypoint_button.js';
import '//resources/cr_components/composebox/composebox_submit.js';
import '//resources/cr_components/composebox/error_scrim.js';
import '//resources/cr_components/composebox/file_carousel.js';
import '//resources/cr_components/search/animated_glow.js';

import type {ComposeboxFile} from '//resources/cr_components/composebox/common.js';
import {getLoadTimeBoolean} from '//resources/cr_components/composebox/common.js';
import type {PageHandlerRemote} from '//resources/cr_components/composebox/composebox.mojom-webui.js';
import type {ComposeboxDropdownElement} from '//resources/cr_components/composebox/composebox_dropdown.js';
import type {ComposeboxFileInputsElement} from '//resources/cr_components/composebox/composebox_file_inputs.js';
import type {ComposeboxInputElement} from '//resources/cr_components/composebox/composebox_input.js';
import {ComposeboxEmbedderMixin, SubmitButtonIconType} from '//resources/cr_components/composebox/composebox_mixin.js';
import {ComposeboxProxyImpl} from '//resources/cr_components/composebox/composebox_proxy.js';
import type {ContextualEntrypointButtonElement} from '//resources/cr_components/composebox/contextual_entrypoint_button.js';
import {HelpBubbleMixinLit} from '//resources/cr_components/help_bubble/help_bubble_mixin_lit.js';
import {GlowAnimationState} from '//resources/cr_components/search/constants.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {AutocompleteResult, PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote, SelectedFileInfo} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {ContextUploadErrorType} from '//resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import {ContextUploadStatus, ToolMode} from '//resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import type {UnguessableToken} from '//resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';

import {OmniboxEverywhereBrowserProxyImpl} from './browser_proxy.js';
import {getCss} from './composebox.css.js';
import {getHtml} from './composebox.html.js';

export interface OmniboxEverywhereComposeboxElement {
  $: {
    composeboxInput: ComposeboxInputElement,
    composebox: HTMLElement,
    matches: ComposeboxDropdownElement,
    fileInputs: ComposeboxFileInputsElement,
  };
}

const OmniboxEverywhereComposeboxElementBase =
    HelpBubbleMixinLit(ComposeboxEmbedderMixin(CrLitElement));

export class OmniboxEverywhereComposeboxElement extends
    OmniboxEverywhereComposeboxElementBase {
  static get is() {
    return 'omnibox-everywhere-composebox';
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
      energyEffectAnimationEnabled: {type: Boolean},
      submitButtonIconType: {type: String},
      clearAllInputsWhenSubmittingQuery: {type: Boolean},
      isScreenshotMenuOpen: {
        type: Boolean,
        reflect: true,
      },
      isContextMenuOpen: {
        type: Boolean,
        reflect: true,
      },
      isPendingScreenshot: {
        type: Boolean,
      },
      isActive: {
        type: Boolean,
        reflect: true,
        attribute: 'is-active',
      },
    };
  }

  accessor isActive: boolean = true;
  accessor isPendingScreenshot: boolean = false;

  /**
   * Entrypoint name used by SearchAnimatedGlowElement and
   * ComposeboxEmbedderMixin to apply embedder-specific styling and themes.
   */
  accessor entrypointName: string = 'OmniboxEverywhere';
  accessor disableComposeboxAnimation: boolean = false;
  accessor applyContextButtonBackground: boolean = false;
  accessor isScreenshotMenuOpen: boolean = false;
  accessor isContextMenuOpen: boolean = false;
  override accessor energyEffectAnimationEnabled: boolean =
      getLoadTimeBoolean('composeboxEnergyEffectAnimationEnabled', true);
  override accessor submitButtonIconType = SubmitButtonIconType.FORWARD;
  // Because Omnibox Everywhere keeps its WebContents alive in the background
  // across hide/show cycles, clear all inputs and attachments upon query
  // submission so subsequent invocations start fresh.
  override accessor clearAllInputsWhenSubmittingQuery: boolean = true;

  override onVoiceSearchButtonClick() {
    this.dispatchEvent(
        new Event('open-voice-search', {bubbles: true, composed: true}));
  }

  private wasScreenshotMenuOpenOnPointerDown_: boolean = false;

  protected onLensSearchPointerdown_(e: PointerEvent) {
    if (e.button !== 0) {
      return;
    }
    this.wasScreenshotMenuOpenOnPointerDown_ = this.isScreenshotMenuOpen;
  }

  protected onLensSearchPointercancel_() {
    this.wasScreenshotMenuOpenOnPointerDown_ = false;
  }

  protected onLensSearchClick_(e: Event) {
    const wasOpen =
        this.wasScreenshotMenuOpenOnPointerDown_ || this.isScreenshotMenuOpen;
    this.wasScreenshotMenuOpenOnPointerDown_ = false;
    if (wasOpen) {
      this.isScreenshotMenuOpen = false;
      return;
    }
    this.notifyHelpBubbleAnchorActivated(
        'kOmniboxEverywhereLensButtonElementId');
    this.hideHelpBubble('kOmniboxEverywhereLensButtonElementId');
    this.isScreenshotMenuOpen = true;
    const anchor = e.currentTarget as HTMLElement;
    const rect = anchor.getBoundingClientRect();
    this.searchboxHandler_.showScreenshotMenu({
      x: Math.round(rect.left),
      y: Math.round(rect.top),
      width: Math.round(rect.width),
      height: Math.round(rect.height),
    });
  }

  protected onContextMenuEntrypointClick_(e?: CustomEvent<{
    anchorRect?: {x: number, y: number, width: number, height: number},
  }>) {
    this.isContextMenuOpen = true;
    const rect = e?.detail?.anchorRect ||
        this.getContextEntrypointElement()?.getBoundingClientRect();
    if (rect) {
      OmniboxEverywhereBrowserProxyImpl.getInstance()
          .handler.showContextActionMenu({
            x: Math.round(rect.x),
            y: Math.round(rect.y),
            width: Math.round(rect.width),
            height: Math.round(rect.height),
          });
    }
  }
  private webuiOmniboxSimplificationEnabled_: boolean =
      getLoadTimeBoolean('webuiOmniboxSimplificationEnabled', false);
  private pageHandler_: PageHandlerRemote;
  private searchboxCallbackRouter_: SearchboxPageCallbackRouter;
  private searchboxHandler_: SearchboxPageHandlerRemote;
  private glowAnimationRafId_: number|null = null;
  private glowAnimationTimeoutId_: number|null = null;

  constructor() {
    super();
    this.pageHandler_ = ComposeboxProxyImpl.getInstance().handler;
    this.searchboxCallbackRouter_ =
        ComposeboxProxyImpl.getInstance().searchboxCallbackRouter;
    this.searchboxHandler_ = ComposeboxProxyImpl.getInstance().searchboxHandler;
  }

  override connectedCallback() {
    super.connectedCallback();
    if (!this.isPendingScreenshot) {
      this.playGlowAnimation();
      this.refreshTabSuggestions(/*forceRefresh=*/ true);
      // Because Omnibox Everywhere sets `searchbox-next-enabled`, the mixin's
      // default `queryZpsOnLoad` in connectedCallback is bypassed. Explicitly
      // query autocomplete here to populate zero-prefix suggestions when
      // opening or re-opening Composebox with an empty query.
      this.queryAutocomplete(/* clearMatches= */ true);
    }
    this.searchboxListenerIds.push(
        this.getSearchboxCallbackRouter().onScreenshotMenuClosed.addListener(
            () => {
              this.isScreenshotMenuOpen = false;
            }));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    if (this.glowAnimationRafId_ !== null) {
      cancelAnimationFrame(this.glowAnimationRafId_);
      this.glowAnimationRafId_ = null;
    }
    if (this.glowAnimationTimeoutId_ !== null) {
      clearTimeout(this.glowAnimationTimeoutId_);
      this.glowAnimationTimeoutId_ = null;
    }
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('inputState')) {
      const inToolMode = this.inputState?.activeTool !== ToolMode.kUnspecified;
      this.applyContextButtonBackground =
          this.webuiOmniboxSimplificationEnabled_ && !inToolMode;
    }

    if (changedProperties.has('isPendingScreenshot')) {
      if (this.isPendingScreenshot) {
        this.showDropdown = false;
        this.result = null;
      } else {
        this.showDropdown = this.computeShowDropdown();
      }
    }
  }

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);
    this.focusInput();
    const lensButton =
        this.shadowRoot?.querySelector<HTMLElement>('#lensSearchButton');
    if (lensButton) {
      this.registerHelpBubble(
          'kOmniboxEverywhereLensButtonElementId', lensButton);
    }
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

  override getContextEntrypointElement(): ContextualEntrypointButtonElement
      |null {
    return this.shadowRoot?.querySelector<ContextualEntrypointButtonElement>(
               '#contextEntrypoint') ||
        null;
  }

  override getFileInputsElement(): ComposeboxFileInputsElement|null {
    return this.shouldDisableFileInputs() ? null : this.$.fileInputs;
  }

  override shouldHideDropdown(): boolean {
    if (this.isPendingScreenshot) {
      return true;
    }
    return super.shouldHideDropdown();
  }

  override computeShowDropdown(): boolean {
    return this.isContextMenuOpen || super.computeShowDropdown();
  }

  override onAutocompleteResultChanged(result: AutocompleteResult) {
    if (this.isPendingScreenshot) {
      // Suppress stale / premature autocomplete results from the previous
      // omnibox session while screenshot capture is pending.
      return;
    }
    super.onAutocompleteResultChanged(result);
  }

  override addFileContextFromBrowser(
      uuid: UnguessableToken, fileInfo: SelectedFileInfo) {
    const wasPendingScreenshot = this.isPendingScreenshot;
    this.isPendingScreenshot = false;
    super.addFileContextFromBrowser(uuid, fileInfo);
    if (wasPendingScreenshot) {
      this.playGlowAnimation();
      this.refreshTabSuggestions(/*forceRefresh=*/ true);
      this.queryAutocomplete(/* clearMatches= */ true);
    }
  }

  override onInputInput(e: CustomEvent<Event>) {
    this.isPendingScreenshot = false;
    super.onInputInput(e);
  }

  onScreenshotCaptureCancelled() {
    this.isPendingScreenshot = false;
    this.refreshTabSuggestions(/*forceRefresh=*/ true);
    this.queryAutocomplete(/* clearMatches= */ true);
  }

  override onContextMenuOpened() {
    super.onContextMenuOpened();
    this.showDropdown = this.computeShowDropdown();
  }

  override async onContextMenuClosed(): Promise<void> {
    this.isContextMenuOpen = false;
    await super.onContextMenuClosed();
    this.showDropdown = this.computeShowDropdown();
  }

  override async keepMenuOpenForMultiSelection(): Promise<void> {
    // Omnibox Everywhere uses a native Views context menu rather than an
    // embedded WebUI menu on the entrypoint button.
  }

  override shouldShowDivider(): boolean {
    if (this.searchboxLayoutMode === 'TallBottomContext' &&
        !this.showFileCarousel) {
      return false;
    }

    return super.shouldShowDivider();
  }

  /**
   * Enables handling of Fusebox suggestion actions (e.g. contextual tool
   * suggestions like "Turn it into a graphic novel illustration" in image mode)
   * so clicking a suggestion chip fills the prompt into the Composebox rather
   * than falling through to standard URL navigation.
   */
  override shouldHandleSuggestionFuseboxActions(): boolean {
    return true;
  }

  override deleteFile(uuidToDelete: UnguessableToken, fromUserAction?: boolean):
      ComposeboxFile|null {
    const file = super.deleteFile(uuidToDelete, fromUserAction);
    if (!file) {
      return null;
    }

    // Refresh autocomplete suggestions when a context chip is removed by the
    // user so stale contextual matches do not linger in the popup dropdown.
    //
    // TODO(b/562109233): Move this logic to the mixin.
    this.queryAutocomplete(/* clearMatches= */ true);
    return file;
  }

  override onContextualInputStatusChanged(
      token: UnguessableToken, status: ContextUploadStatus,
      errorType: ContextUploadErrorType|null) {
    super.onContextualInputStatusChanged(token, status, errorType);
    // When a tab/file context is unselected or removed via the browser-side
    // context menu, C++ sends `kUploadReplaced`. Omnibox Everywhere needs to
    // explicitly query autocomplete with `clearMatches = true` to refresh
    // suggestions so stale contextual results for the unselected context do
    // not linger. We keep this behavior specific to Omnibox Everywhere to avoid
    // unintentionally triggering re-queries or disrupting other consumers
    // (such as NTP or Lens) that are not reinvoked in the same manner when a
    // new tab or context is added or removed.
    if (status === ContextUploadStatus.kUploadReplaced) {
      this.queryAutocomplete(/* clearMatches= */ true);
    }
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
    if (this.attachedContext.size > 0 &&
        Array.from(this.attachedContext.values())
            .some(file => file.supportsUnimodal)) {
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

  setInputText(text: string) {
    this.input = text;
    const inputElem = this.getInputElement();
    if (inputElem) {
      inputElem.input = text;
    }
  }

  playGlowAnimation(timeoutMs: number = 1000) {
    if (this.glowAnimationRafId_ !== null) {
      cancelAnimationFrame(this.glowAnimationRafId_);
      this.glowAnimationRafId_ = null;
    }
    if (this.glowAnimationTimeoutId_ !== null) {
      clearTimeout(this.glowAnimationTimeoutId_);
      this.glowAnimationTimeoutId_ = null;
    }
    this.animationState = GlowAnimationState.NONE;
    this.glowAnimationRafId_ = requestAnimationFrame(() => {
      this.glowAnimationRafId_ = null;
      this.animationState = GlowAnimationState.EXPANDING;
      this.glowAnimationTimeoutId_ = setTimeout(() => {
        if (this.animationState === GlowAnimationState.EXPANDING) {
          this.animationState = GlowAnimationState.NONE;
        }
        this.glowAnimationTimeoutId_ = null;
      }, timeoutMs);
    });
  }

  override handleEscapeKeyLogic() {
    this.onCancelClick();
  }
}


declare global {
  interface HTMLElementTagNameMap {
    'omnibox-everywhere-composebox': OmniboxEverywhereComposeboxElement;
  }
}

customElements.define(
    OmniboxEverywhereComposeboxElement.is, OmniboxEverywhereComposeboxElement);
