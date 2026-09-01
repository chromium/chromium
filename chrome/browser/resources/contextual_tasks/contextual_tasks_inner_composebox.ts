// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_components/composebox/composebox_dropdown.js';
import '//resources/cr_components/composebox/composebox_file_inputs.js';
import '//resources/cr_components/composebox/composebox_input.js';
import '//resources/cr_components/composebox/composebox_submit.js';
import '//resources/cr_components/composebox/composebox_tool_chip.js';
import '//resources/cr_components/composebox/composebox_voice_search.js';
import '//resources/cr_components/composebox/contextual_entrypoint_and_menu.js';
import '//resources/cr_components/composebox/error_scrim.js';
import '//resources/cr_components/composebox/file_carousel.js';
import '//resources/cr_components/search/animated_glow.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {ComposeboxFile, GlifAnimationState, recordBoolean, recordUserAction, TabUploadOrigin} from '//resources/cr_components/composebox/common.js';
import type {TabUpload} from '//resources/cr_components/composebox/common.js';
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
import {loadTimeData} from '//resources/js/load_time_data.js';
import {debounceEnd} from '//resources/js/util.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {AutocompleteResult, PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote, TabInfo} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
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
  clearInputsForNewThread(): void;
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
      entrypointName: {type: String, reflect: true},
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
  accessor entrypointName: string = 'ContextualTasks';
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
  private readonly smartTabSharingSupported_: boolean =
      loadTimeData.getBoolean('composeboxSmartTabSharingSupported');

  private get webUIOmniboxAskGAboutThisPageEnabled_(): boolean {
    return loadTimeData.valueExists('webUIOmniboxAskGAboutThisPageEnabled') &&
        loadTimeData.getBoolean('webUIOmniboxAskGAboutThisPageEnabled');
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

  override getLensButtonElement(): HTMLElement|null {
    return this.shadowRoot?.querySelector('#lensIcon') || null;
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
    this.searchboxListenerIds.push(
        this.getSearchboxCallbackRouter()
            .updateAutoSuggestedTabContext.addListener(
                this.updateAutoSuggestedTabContext_.bind(this)));
    this.focusInput();
    // firstUpdated() runs only once, so restore the observers on reconnect (
    // the shadow DOM persists); the initial setup happens in firstUpdated().
    if (this.hasUpdated) {
      this.syncResizeObservers_();
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

    if (!this.hasUpdated) {
      // The mixin default reads the all-surfaces coherence key; Contextual
      // Tasks must use the cobrowsing-specific key.
      this.voiceSearchCoherenceEnabled = loadTimeData.getBoolean(
          'voiceSearchCoherenceCobrowsingComposeboxEnabled');
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

  override computeVoiceSearchCoherenceEnabled(): boolean {
    return loadTimeData.getBoolean(
        'voiceSearchCoherenceCobrowsingComposeboxEnabled');
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
        !this.submitting && result.queryId === this.activeQueryId;
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
    if (!this.smartTabSharingSupported_) {
      return;
    }
    super.onSmartTabSharingActiveChanged(e);
  }

  private async updateAutoSuggestedTabContext_(
      tab: TabInfo|null, invocationSource: string|null) {
    if (this.smartTabSharingSupported_ && this.smartTabSharingActive) {
      if (this.automaticActiveTab) {
        this.deleteFile(this.automaticActiveTab.uuid);
        this.automaticActiveTab = null;
      }
      return;
    }
    // AutoSuggestedTabContext is routed differently for Omnibox Page Action.
    // when it opens a side panel to cobrowse.
    const askGAndPageAction = this.webUIOmniboxAskGAboutThisPageEnabled_ &&
        invocationSource === 'OmniboxPageAction' && this.isSidePanel;

    // We should delete the automatic active tab if it is different from the
    // current tab when webUIOmniboxAskGAboutThisPageEnabled_ is true. Make sure
    // to keep the existing tab if we are returning from another tab.
    const hasTabMismatch = !!this.automaticActiveTab && !!tab &&
        this.automaticActiveTab.url !== tab.url;
    const shouldDeleteAutomaticActiveTab = askGAndPageAction ?
        hasTabMismatch :
        this.automaticActiveTab && (!tab || hasTabMismatch);

    if (shouldDeleteAutomaticActiveTab) {
      this.deleteFile(this.automaticActiveTab!.uuid);
      this.automaticActiveTab = null;
      this.pendingAutomaticActiveTabUrl = '';
      this.pendingAutomaticActiveTabTitle = '';

      // TODO(crbug.com/482150500): Correctly query for url based suggestions
      // when delayed tab is present. Right now, while url-based suggestions are
      // not set-up, clear the autocomplete matches.
      if (!askGAndPageAction && !tab) {
        this.queryAutocomplete(/* clearMatches= */ true);
      }
      return;
    }

    if (!tab) {
      this.pendingAutomaticActiveTabUrl = '';
      this.pendingAutomaticActiveTabTitle = '';
      return;
    }

    if (tab) {
      // Ignore the `TabInfo` update if there is a matching
      // `automaticActiveTab`, unless the title has changed.
      if (this.automaticActiveTab && tab.url === this.automaticActiveTab.url &&
          tab.tabId === this.automaticActiveTab.tabId) {
        if (this.automaticActiveTab.name !== tab.title) {
          const updatedFile = new ComposeboxFile(
              this.automaticActiveTab.uuid, tab.title,
              this.automaticActiveTab.type, this.automaticActiveTab.inputType,
              this.automaticActiveTab);
          this.automaticActiveTab = updatedFile;
          const fileMap = new Map(this.attachedContext);
          fileMap.set(updatedFile.uuid, updatedFile);
          this.attachedContext = fileMap;
        }
        return;
      }

      // If an autochip is currently being uploaded but carousel attachment has
      // not been created yet, allow updates to its title. Absence of this
      // url means that there is no currently no auto active tab uploading.
      // If the url is the same, this is an update for the same tab so just
      // allow updates to the uploading tab's title from this update,
      // but do not upload it again.
      if (this.pendingAutomaticActiveTabUrl === tab.url) {
        this.pendingAutomaticActiveTabTitle = tab.title;
        return;
      }
      // Otherwise, prepare to replace the auto chip:
      this.pendingAutomaticActiveTabUrl = tab.url;
      this.pendingAutomaticActiveTabTitle = tab.title;

      // Do not reset above pending states in this async callback since
      // later requests make any older async callback updates irrelevant.
      // Add the `TabInfo` as `ComposeboxFile` in carousel.
      const attachment = await this.addTabContextHandleCallback(
          {
            tabId: tab.tabId,
            title: tab.title,
            url: tab.url,
            delayUpload: !askGAndPageAction,
            origin: TabUploadOrigin.AUTO_ACTIVE,
          } as TabUpload,
          /*replaceAutoActiveTabToken=*/ true);

      if (!askGAndPageAction || !attachment) {
        this.clearAutocompleteMatches();
      }
    }
  }

  override async addTabContextHandleCallback(
      tabUpload: TabUpload, replaceAutoActiveTabToken: boolean = false):
      Promise<ComposeboxFile|null> {
    const attachment = await super.addTabContextHandleCallback(
        tabUpload, replaceAutoActiveTabToken, (attachment) => {
          // Do not reset pending active tab to avoid overwriting
          // synchronous "pending statuses" that are queued (since this
          // function is asynchronous and can run much later).
          if (replaceAutoActiveTabToken) {
            this.automaticActiveTab =
                Object.assign(attachment, {uuid: attachment.uuid});
          }
        });

    if (!attachment) {
      return null;
    }
    // Adding a tab is asynchronous. For auto active tabs, a title update
    // might be received after the upload process has been started. In order
    // to prevent adding duplicate chips from this update, simply update the
    // title of the initial upload instead based on whatever the latest
    // title update received is.
    if (replaceAutoActiveTabToken && this.automaticActiveTab) {
      if (this.automaticActiveTab.name !==
          this.pendingAutomaticActiveTabTitle) {
        const updatedFile = new ComposeboxFile(
            this.automaticActiveTab.uuid, this.pendingAutomaticActiveTabTitle,
            this.automaticActiveTab.type, this.automaticActiveTab.inputType,
            this.automaticActiveTab);
        this.automaticActiveTab = updatedFile;
        const fileMap = new Map(this.attachedContext);
        fileMap.set(updatedFile.uuid, updatedFile);
        this.attachedContext = fileMap;
      }
    }
    return attachment;
  }

  override deleteFile(uuidToDelete: UnguessableToken, fromUserAction?: boolean):
      ComposeboxFile|null {
    const fromAutoSuggestedChip =
        uuidToDelete === this.automaticActiveTab?.uuid &&
        (fromUserAction === true);
    const file =
        super.deleteFile(uuidToDelete, fromUserAction, fromAutoSuggestedChip);

    if (!file) {
      return null;
    }

    if (fromAutoSuggestedChip) {
      // TODO(crbug.com/492797638): Consider folding this into the
      // `InputStateDeletion` metric.
      const metricName = 'ContextualSearch.UserAction.DeleteAutoSuggestedTab.' +
          this.composeboxSource;
      recordUserAction(metricName);
      recordBoolean(metricName, true);
      this.automaticActiveTab = null;
      this.pendingAutomaticActiveTabUrl = '';
      this.pendingAutomaticActiveTabTitle = '';
    }
    // We should not be querying autocomplete in the presence of a tab
    // with delayed upload until URL suggestions are implemented.
    // `deleteContext_` gets called before the active tab chip token is cleared,
    // therefore, check if we're removing this chip to see if the delayed tab
    // is getting removed.
    if (fromAutoSuggestedChip || !this.getHasAutomaticActiveTabChipToken()) {
      this.queryAutocomplete(/* clearMatches= */ true);
    } else {
      // TODO(crbug.com/482150500): Have URL-suggestions for tabs with delayed
      // uploads.
      this.clearAutocompleteMatches();
    }
    return file;
  }

  override clearAllInputs(
      querySubmitted: boolean, shouldBlockAutoSuggestedTabs: boolean) {
    // Reset side-panel specific suggested tab context URL/Title pointers
    this.automaticActiveTab = null;
    this.pendingAutomaticActiveTabUrl = '';
    this.pendingAutomaticActiveTabTitle = '';
    super.clearAllInputs(querySubmitted, shouldBlockAutoSuggestedTabs);
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

    // The file hint should only be shown when there is context that was
    // deliberately added by the user (i.e. not the automatic active tab).
    const isOnlyAutoTab = this.attachedContext.size === 1
        && !!this.automaticActiveTab;
    const shouldUseFileHint = this.enableFileHint && this.hasFiles() &&
        !isOnlyAutoTab && this.inputState?.activeTool === ToolMode.kUnspecified;
    if (shouldUseFileHint) {
      if (this.attachedContext.size > 1) {
        this.inputPlaceholder = this.i18n('composeboxHintTextAskAboutThese');
        return;
      }
      const file = this.attachedContext.values().next().value!;
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
    const hasNonTabFiles =
        Array.from(this.attachedContext.values()).some(f => !f.url);
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
    if (!this.automaticActiveTab) {
      return null;
    }
    const carousel =
        this.shadowRoot?.querySelector<ComposeboxFileCarouselElement>(
            '#carousel');
    if (!carousel) {
      return null;
    }

    return carousel.getThumbnailElementByUuid(this.automaticActiveTab.uuid);
  }

  getHasAutomaticActiveTabChipToken(): boolean {
    return this.automaticActiveTab !== null;
  }

  injectInput(
      title: string, thumbnail: string, fileToken: UnguessableToken,
      supportsUnimodal: boolean, iconName?: string): void {
    const attachment = ComposeboxFile.createFromInjectedInput(
        fileToken, thumbnail, title, iconName ?? null);
    attachment.supportsUnimodal = supportsUnimodal;

    this.onFileContextAdded(attachment);
  }

  setInputProgrammatically(
      queryText: string, willSubmitAfterInjection: boolean): void {
    this.input = queryText;

    if (!willSubmitAfterInjection) {
      // If not submitting immediately, suggestions for the new input might be
      // desired.
      this.queryAutocomplete(/*clearMatches=*/ true);
      return;
    }

    // Stop any in-flight autocomplete queries to prevent unnecessary backend
    // work for a query that is about to be submitted.
    this.getSearchboxHandler().stopAutocomplete(/*clearResult=*/ true);

    this.lastQueriedInput = '';

    // Hide the dropdown and drop any stale matches. This also resets
    // `activeQueryId` to -1, so autocomplete results that still arrive for
    // earlier queries are rejected by the query-ID guard in
    // `onAutocompleteResultChanged`.
    this.clearAutocompleteMatches();
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
