// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_components/composebox/composebox_dropdown.js';
import '//resources/cr_components/composebox/composebox_file_inputs.js';
import '//resources/cr_components/composebox/composebox_input.js';
import '//resources/cr_components/composebox/composebox_submit.js';
import '//resources/cr_components/composebox/composebox_tool_chip.js';
import '//resources/cr_components/composebox/contextual_entrypoint_and_menu.js';
import '//resources/cr_components/composebox/error_scrim.js';
import '//resources/cr_components/composebox/file_carousel.js';
import '//resources/cr_components/search/animated_glow.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {GlifAnimationState} from '//resources/cr_components/composebox/common.js';
import type {ComposeboxFile} from '//resources/cr_components/composebox/common.js';
import type {PageHandlerRemote} from '//resources/cr_components/composebox/composebox.mojom-webui.js';
import type {ComposeboxDropdownElement} from '//resources/cr_components/composebox/composebox_dropdown.js';
import type {ComposeboxFileInputsElement} from '//resources/cr_components/composebox/composebox_file_inputs.js';
import type {ComposeboxInputElement} from '//resources/cr_components/composebox/composebox_input.js';
import {ComposeboxEmbedderMixin} from '//resources/cr_components/composebox/composebox_mixin.js';
import {ComposeboxProxyImpl} from '//resources/cr_components/composebox/composebox_proxy.js';
import {ToolMode} from '//resources/cr_components/composebox/composebox_query.mojom-webui.js';
import type {ContextualEntrypointAndMenuElement} from '//resources/cr_components/composebox/contextual_entrypoint_and_menu.js';
import type {ErrorScrimElement} from '//resources/cr_components/composebox/error_scrim.js';
import type {ComposeboxFileCarouselElement} from '//resources/cr_components/composebox/file_carousel.js';
import type {GlowAnimationState} from '//resources/cr_components/search/constants.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {debounceEnd} from '//resources/js/util.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {AutocompleteResult, PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {UnguessableToken} from '//resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';

import {getCss} from './contextual_tasks_inner_composebox.css.js';
import {getHtml} from './contextual_tasks_inner_composebox.html.js';

// Debounce interval for the ResizeObserver callbacks that fire
// `composebox-resize`
const RESIZE_EVENT_DEBOUNCE_TIMEOUT_MS = 20;

// Inner-element contract the `contextual-tasks-composebox` wrapper invokes on
// its `#composebox` child; both `<cr-composebox>` and this fork satisfy it.
export interface ContextualTasksInnerComposeboxInterface {
  animationState: GlowAnimationState;
  canSubmitFilesAndInput: boolean;
  clearAllInputsWhenSubmittingQuery: boolean;
  disableCaretColorAnimation: boolean;
  disableFallbackGlifAnimation: boolean;
  dropdownNeeded: boolean;
  enableCarouselScrolling: boolean;
  enableFileHint: boolean;
  energyEffectAnimationEnabled: boolean;
  energyEffectEnabled: boolean;
  glifAnimationState: GlifAnimationState;
  input: string;
  inputPlaceholderOverride: string;
  isCanvasQuerySubmitted: boolean;
  isFollowupQuery: boolean;
  isSidePanel: boolean;
  isZeroState: boolean;
  lensButtonDisabled: boolean;
  lensButtonTriggersOverlay: boolean;
  queryZpsOnLoad: boolean;
  searchboxLayoutMode: string;
  showLensButton: boolean;
  showVoiceSearch: boolean;
  readonly updateComplete: Promise<boolean>;
  usePecApi: boolean;

  clearAllInputs(
      querySubmitted: boolean, shouldBlockAutoSuggestedTabs: boolean): void;
  clearAutocompleteMatches(): void;
  deleteFile(
      uuidToDelete: UnguessableToken, fromUserAction?: boolean,
      fromAutoSuggestedChip?: boolean): ComposeboxFile|null;
  focusInput(): void;
  getAutomaticActiveTabChipElement(): HTMLElement|null;
  getDropTarget(): HTMLElement;
  getHasAutomaticActiveTabChipToken(): boolean;
  hasFiles(): boolean;
  injectInput(
      title: string, thumbnail: string, fileToken: UnguessableToken,
      supportsUnimodal: boolean, iconName?: string): void;
  queryAutocomplete(clearMatches: boolean): void;
  setInputProgrammatically(
      queryText: string, willSubmitAfterInjection: boolean): void;
  submitQuery(e?: KeyboardEvent|MouseEvent): void;
}

export interface ContextualTasksInnerComposeboxElement {
  $: {
    composeboxInput: ComposeboxInputElement,
    composebox: HTMLElement,
    matches: ComposeboxDropdownElement,
    fileInputs: ComposeboxFileInputsElement,
    carousel: ComposeboxFileCarouselElement,
    errorScrim: ErrorScrimElement,
  };
}

export class
    ContextualTasksInnerComposeboxElement extends ComposeboxEmbedderMixin
(CrLitElement) implements ContextualTasksInnerComposeboxInterface {
  static get is() {
    return 'contextual-tasks-inner-composebox';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      carouselOnTop_: {type: Boolean},
      disableFallbackGlifAnimation: {type: Boolean},
      enableCarouselScrolling: {type: Boolean},
      enableFileHint: {type: Boolean},
      glifAnimationState: {type: String},
      inputPlaceholderOverride: {type: String},
      isFollowupQuery: {type: Boolean},
      isSidePanel: {type: Boolean},
      isZeroState: {type: Boolean},
      lensButtonDisabled: {
        reflect: true,
        type: Boolean,
      },
      lensButtonTriggersOverlay: {type: Boolean},
      showLensButton: {type: Boolean},
      expanding_: {
        reflect: true,
        type: Boolean,
      },
    };
  }

  // Wrapper-bound properties.
  accessor disableFallbackGlifAnimation: boolean = false;
  accessor enableCarouselScrolling: boolean = true;
  accessor enableFileHint: boolean = false;
  accessor glifAnimationState: GlifAnimationState =
      GlifAnimationState.INELIGIBLE;
  accessor inputPlaceholderOverride: string = '';
  accessor isFollowupQuery: boolean = false;
  accessor isSidePanel: boolean = false;
  accessor isZeroState: boolean = true;
  accessor lensButtonDisabled: boolean = false;
  accessor lensButtonTriggersOverlay: boolean = false;
  accessor showLensButton: boolean = true;

  protected accessor carouselOnTop_: boolean = false;
  // Reflected: the wrapper CSS and imported shared composebox.css key
  // [expanding_] rules on it. Contextual Tasks is never collapsible.
  protected accessor expanding_: boolean = true;

  private searchboxCallbackRouter_: SearchboxPageCallbackRouter;
  private pageHandler_: PageHandlerRemote;
  private searchboxHandler_: SearchboxPageHandlerRemote;
  private eventTracker_: EventTracker = new EventTracker();
  private resizeObservers_: ResizeObserver[] = [];

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

  override async connectedCallback() {
    super.connectedCallback();
    this.focusInput();
    // firstUpdated() runs only once, so restore the observers on reconnect (
    // the shadow DOM persists); the initial setup happens in firstUpdated().
    if (this.hasUpdated) {
      this.syncResizeObservers_();
    }
    if (this.smartTabSharingVisible) {
      const {active} = await this.pageHandler_.getSmartTabSharingActive();
      this.smartTabSharingActive = active;
      if (active) {
        this.clearContextForSmartTabSharingActive_();
      }
    }
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
    this.tearDownResizeObservers_();
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    if (changedProperties.has('inputPlaceholderOverride') ||
        changedProperties.has('enableFileHint')) {
      this.updateInputPlaceholder();
    }
  }

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);
    // Set up after the first render so `this.$.matches` exists; CT is the only
    // embedder that consumes these resize events.
    this.syncResizeObservers_();
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    if (changedProperties.has('result') ||
        changedProperties.has('showDropdown')) {
      // Fires `show-suggestion-activity-link`; the wrapper owns the link UI.
      this.shouldShowSuggestionActivityLink();
    }
  }

  private setupResizeObservers_() {
    const composeboxResizeObserver = new ResizeObserver(debounceEnd(() => {
      this.fire('composebox-resize', {height: this.offsetHeight});
    }, RESIZE_EVENT_DEBOUNCE_TIMEOUT_MS));
    this.resizeObservers_.push(composeboxResizeObserver);
    composeboxResizeObserver.observe(this);

    const composeboxDropdownResizeObserver =
        new ResizeObserver(debounceEnd(() => {
          this.fire(
              'composebox-resize',
              {dropdownHeight: this.$.matches.offsetHeight});
        }, RESIZE_EVENT_DEBOUNCE_TIMEOUT_MS));
    this.resizeObservers_.push(composeboxDropdownResizeObserver);
    composeboxDropdownResizeObserver.observe(this.$.matches);
  }

  private tearDownResizeObservers_() {
    for (const observer of this.resizeObservers_) {
      observer.disconnect();
    }
    this.resizeObservers_ = [];
  }

  private syncResizeObservers_() {
    this.tearDownResizeObservers_();
    if (!this.isConnected) {
      return;
    }
    this.setupResizeObservers_();
  }

  override onAutocompleteResultChanged(result: AutocompleteResult) {
    // Reuse the mixin's dropdown/selection logic, but notify the wrapper via
    // `result-changed` only for accepted results (mirrors cr-composebox).
    const isValidResult =
        !this.submitting && this.lastQueriedInput.trimStart() === result.input;
    if (isValidResult && this.composeboxNoFlickerSuggestionsFix &&
        this.showTypedSuggest &&
        !this.haveReceivedSynchronousAutocompleteResponse) {
      // First typed-suggest response can collapse the dropdown; carry over
      // the prior non-verbatim matches.
      if (this.result && this.result.matches.length > 0 &&
          result.matches.length <= 1) {
        result.matches.push(...this.result.matches.filter(
            match => match.type !== 'search-what-you-typed'));
      }
      this.haveReceivedSynchronousAutocompleteResponse = true;
    }
    super.onAutocompleteResultChanged(result);
    if (isValidResult) {
      this.fire('result-changed', result);
    }
  }

  override onSmartTabSharingActiveChanged(e: CustomEvent<{active: boolean}>) {
    super.onSmartTabSharingActiveChanged(e);
    if (e.detail.active) {
      this.clearContextForSmartTabSharingActive_();
    }
  }

  private clearContextForSmartTabSharingActive_() {
    // TODO(crbug.com/486707842): Also clear the automatic active tab once it
    // is migrated into this fork.
    this.clearManualTabs_();
  }

  private clearManualTabs_() {
    const fileMap = new Map(this.files);
    for (const [uuid, file] of fileMap.entries()) {
      if (file.type === 'tab') {
        this.deleteFile(uuid, /*fromUserAction=*/ false);
      }
    }
  }


  protected onComposeboxFocusin_(e: FocusEvent) {
    // Exit early if the focus is still within the composebox.
    if (this.$.composebox.contains(e.relatedTarget as Node)) {
      return;
    }
    this.pageHandler_.focusChanged(true);
    this.fire('composebox-focus-in');
  }

  protected onComposeboxFocusout_(e: FocusEvent) {
    // Exit early if the focus is still within the composebox.
    if (this.$.composebox.contains(e.relatedTarget as Node)) {
      return;
    }
    this.pageHandler_.focusChanged(false);
    this.fire('composebox-focus-out');
  }

  protected onLensClick_() {
    if (this.lensButtonTriggersOverlay) {
      this.pageHandler_.handleLensButtonClick();
    } else {
      this.pageHandler_.handleFileUpload(/*is_image=*/ true);
    }
  }

  protected onLensIconMousedown_(e: MouseEvent) {
    // Capture the mousedown so clicking the Lens icon does not focus (and
    // expand) the composebox.
    e.preventDefault();
  }

  override updateInputPlaceholder() {
    if (this.inputPlaceholderOverride) {
      this.inputPlaceholder = this.inputPlaceholderOverride;
      return;
    }

    const shouldUseFileHint = this.enableFileHint && this.hasFiles() &&
        this.inputState?.activeTool === ToolMode.kUnspecified;
    if (shouldUseFileHint) {
      if (this.files.size > 1) {
        this.inputPlaceholder = this.i18n('composeboxHintTextAskAboutThese');
        return;
      }
      const file = this.files.values().next().value!;
      if (file.type === 'tab') {
        this.inputPlaceholder = this.i18n('composeboxHintTextAskAboutThisTab');
        return;
      } else if (file.type.includes('image')) {
        this.inputPlaceholder =
            this.i18n('composeboxHintTextAskAboutThisImage');
        return;
      } else if (file.type === 'pdf' || file.type === 'application/pdf') {
        this.inputPlaceholder = this.i18n('composeboxHintTextAskAboutThisDoc');
        return;
      }
    }

    super.updateInputPlaceholder();
  }

  override hasValidQuery(): boolean {
    // TODO(crbug.com/485648942): Update to drive Deep Search behavior from the
    // PEC API's ToolSubstateConfig.
    // Allow an empty query for Deep Search follow-ups; super handles files,
    // selected matches, and non-empty text.
    return super.hasValidQuery() ||
        (this.inputState?.activeTool === ToolMode.kDeepSearch &&
         this.isFollowupQuery);
  }

  override shouldShowDivider(): boolean {
    // Retain the divider when only tab favicons are present.
    const hasNonTabFiles = Array.from(this.files.values()).some(f => !f.url);
    if (this.hasTabs() && !hasNonTabFiles) {
      return this.showDropdown;
    }
    return super.shouldShowDivider();
  }

  override shouldDisableFileInputs(): boolean {
    // Contextual Tasks never uploads through the hidden file inputs; uploads
    // go through the browser handler (Lens, drag/drop, paste).
    return true;
  }

  getAutomaticActiveTabChipElement(): HTMLElement|null {
    // TODO: Migrate automatic active tab behavior.
    return null;
  }

  getHasAutomaticActiveTabChipToken(): boolean {
    // TODO: Migrate automatic active tab behavior.
    return false;
  }

  injectInput(
      _title: string, _thumbnail: string, _fileToken: UnguessableToken,
      _supportsUnimodal: boolean, _iconName?: string): void {
    // TODO: Migrate inject-input behavior.
  }

  setInputProgrammatically(
      _queryText: string, _willSubmitAfterInjection: boolean): void {
    // TODO: Migrate set-input-programmatically behavior.
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'contextual-tasks-inner-composebox': ContextualTasksInnerComposeboxElement;
  }
}

customElements.define(
    ContextualTasksInnerComposeboxElement.is,
    ContextualTasksInnerComposeboxElement);
