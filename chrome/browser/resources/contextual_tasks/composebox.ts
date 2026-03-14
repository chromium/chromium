// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_components/composebox/composebox_dropdown.js';
import '//resources/cr_components/composebox/composebox.js';
import '//resources/cr_components/localized_link/localized_link.js';

import type {ComposeboxElement} from '//resources/cr_components/composebox/composebox.js';
import type {PageHandlerRemote} from '//resources/cr_components/composebox/composebox.mojom-webui.js';
import type {ComposeboxDropdownElement} from '//resources/cr_components/composebox/composebox_dropdown.js';
import {ComposeboxProxyImpl, createAutocompleteMatch} from '//resources/cr_components/composebox/composebox_proxy.js';
import {GlowAnimationState} from '//resources/cr_components/search/constants.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {AutocompleteMatch, AutocompleteResult, PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {ToolMode} from '//resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import type {UnguessableToken} from '//resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './composebox.css.js';
import {getHtml} from './composebox.html.js';
import {VoiceSearchState} from './constants.js';
import {IconType} from './contextual_tasks.mojom-webui.js';

const ICON_TYPE_TO_NAME: {[id: number]: string} = {
  [IconType.kUnspecified]: 'unspecified',
  [IconType.kAdd]: 'add',
  [IconType.kFormatQuoteFilled]: 'quoteFilled',
  [IconType.kImage]: 'image',
  [IconType.kDrivePdf]: 'drivePdf',
  [IconType.kCheck]: 'check',
};

function recordVoiceSearchAction(voiceSearchState: VoiceSearchState) {
  // Safety return statement in rare case chrome metrics is not available.
  if (!chrome.metricsPrivate) {
    return;
  }

  chrome.metricsPrivate.recordEnumerationValue(
      'ContextualTasks.VoiceSearch.State', voiceSearchState,
      VoiceSearchState.MAX_VALUE + 1);
}

function createGhostMatch(): AutocompleteMatch {
  return createAutocompleteMatch({
    contents: '\u200b',
    description: '\u200b',
    type: 'SEARCH_SUGGEST',
    isSearchType: true,
    iconPath: '//resources/cr_components/searchbox/icons/search_spark.svg',
  });
}
export interface ContextualTasksComposeboxElement {
  $: {
    composebox: ComposeboxElement,
    composeboxContainer: HTMLElement,
    contextualTasksSuggestionsContainer: ComposeboxDropdownElement,
  };
}

export class ContextualTasksComposeboxElement extends I18nMixinLit
(CrLitElement) {
  static get is() {
    return 'contextual-tasks-composebox';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      enableNativeZeroStateSuggestions: {type: Boolean},
      isZeroState: {
        type: Boolean,
        reflect: true,
      },
      isSidePanel: {
        type: Boolean,
        reflect: true,
      },
      isLensOverlayShowing: {
        type: Boolean,
        reflect: true,
      },
      maybeShowOverlayHintText: {
        type: Boolean,
        reflect: true,
      },
      composeboxHeight_: {type: Number},
      composeboxDropdownHeight_: {type: Number},
      isComposeboxFocused_: {
        type: Boolean,
        reflect: true,
      },
      showContextMenu_: {
        type: Boolean,
        value: loadTimeData.getBoolean('composeboxShowContextMenu'),
      },
      zeroStateSuggestions_: {type: Object},
      activeToolMode_: {
        type: Number,
      },
      isLoading_: {
        type: Boolean,
        reflect: true,
      },
      inputEnabled: {
        type: Boolean,
        reflect: true,
      },
      showSuggestionsActivityLink_: {
        type: Boolean,
        reflect: true,
      },
      selectedMatchIndex_: {type: Number},
    };
  }

  accessor enableNativeZeroStateSuggestions: boolean = false;
  accessor isZeroState: boolean = false;
  accessor isSidePanel: boolean = false;
  accessor isLensOverlayShowing: boolean = false;
  accessor maybeShowOverlayHintText: boolean = false;
  accessor inputEnabled: boolean = true;

  protected accessor zeroStateSuggestions_: AutocompleteResult = {
    input: '',

    suggestionGroupsMap: {},

    matches: Array(5).fill(null).map(() => createGhostMatch()),

    smartComposeInlineHint: null,
  };
  /* If suggestions are loading. Set this any time that should hide suggestions
   * while load next set of suggestions (after attaching image, etc.)
   */
  accessor isLoading_ = true;
  protected accessor composeboxHeight_: number = 0;
  protected accessor composeboxDropdownHeight_: number = 0;
  protected accessor isComposeboxFocused_: boolean = false;
  protected accessor showContextMenu_: boolean =
      loadTimeData.getBoolean('composeboxShowContextMenu');
  protected accessor activeToolMode_: ToolMode = ToolMode.kUnspecified;
  protected accessor showSuggestionsActivityLink_: boolean = false;
  protected accessor selectedMatchIndex_: number = -1;
  protected searchboxHandler_: SearchboxPageHandlerRemote;
  private eventTracker_: EventTracker = new EventTracker();
  private pageHandler_: PageHandlerRemote;
  private searchboxCallbackRouter_: SearchboxPageCallbackRouter;
  private searchboxListenerIds_: number[] = [];
  // Tracks the resize of the composebox to provide height updates.
  private resizeObserver_: ResizeObserver|null = null;
  protected caretAnimationsEnabled_: boolean =
      loadTimeData.getBoolean('caretAnimationEnabled');

  constructor() {
    super();
    this.pageHandler_ = ComposeboxProxyImpl.getInstance().handler;
    this.searchboxCallbackRouter_ =
        ComposeboxProxyImpl.getInstance().searchboxCallbackRouter;
    this.searchboxHandler_ = ComposeboxProxyImpl.getInstance().searchboxHandler;
  }

  override connectedCallback() {
    super.connectedCallback();

    const composebox = this.$.composebox;
    if (composebox) {
      this.eventTracker_.add(composebox, 'composebox-focus-in', () => {
        this.isComposeboxFocused_ = true;
      });
      this.eventTracker_.add(composebox, 'composebox-focus-out', () => {
        this.isComposeboxFocused_ = false;
        if (composebox.animationState === GlowAnimationState.SUBMITTING ||
            composebox.animationState === GlowAnimationState.LISTENING) {
          return;
        }
        composebox.animationState = GlowAnimationState.NONE;
      });
      this.eventTracker_.add(composebox, 'composebox-submit', () => {
        this.clearInputAndFocus(/* querySubmitted= */ true);
      });
      this.eventTracker_.add(
          composebox, 'carousel-resize', (e: CustomEvent) => {
            if (e.detail.height !== undefined) {
              composebox.style.setProperty(
                  '--carousel-height', `${e.detail.height}px`);
              this.fire('update-tooltip-visibility');
            }
          });
      this.eventTracker_.add(
          composebox, 'composebox-resize', (e: CustomEvent) => {
            if (e.detail.height !== undefined) {
              this.composeboxHeight_ = e.detail.height;
            }
          });

      this.eventTracker_.add(
          composebox.getDropTarget(), 'on-context-files-changed', () => {
            this.fire('update-tooltip-visibility');
          });

      this.eventTracker_.add(
          composebox, 'composebox-voice-search-start', () => {
            recordVoiceSearchAction(
                VoiceSearchState.VOICE_SEARCH_BUTTON_CLICKED);
          });

      this.eventTracker_.add(
          composebox, 'composebox-voice-search-transcription-success', () => {
            recordVoiceSearchAction(VoiceSearchState.SUCCESSFUL_TRANSCRIPT);
          });

      this.eventTracker_.add(
          composebox, 'composebox-voice-search-error', () => {
            recordVoiceSearchAction(VoiceSearchState.VOICE_SEARCH_ERROR);
          });
      this.eventTracker_.add(
          composebox, 'composebox-voice-search-error-and-canceled', () => {
            recordVoiceSearchAction(
                VoiceSearchState.VOICE_SEARCH_ERROR_AND_CANCELED);
          });
      this.eventTracker_.add(
          composebox, 'active-tool-mode-changed',
          (e: CustomEvent<{value: ToolMode}>) => {
            this.activeToolMode_ = e.detail.value;
          });
      this.eventTracker_.add(
          composebox, 'composebox-voice-search-user-canceled', () => {
            recordVoiceSearchAction(VoiceSearchState.VOICE_SEARCH_CANCELED);
          });
      this.activeToolMode_ = composebox.activeToolMode;

      this.fire('update-tooltip-visibility', {height: composebox.offsetHeight});

      this.resizeObserver_ = new ResizeObserver(() => {
        this.composeboxHeight_ = composebox.offsetHeight;
        this.dispatchEvent(new CustomEvent('composebox-height-update', {
          bubbles: true,
          composed: true,
          detail: {height: this.composeboxHeight_},
        }));
      });
      this.resizeObserver_.observe(composebox);
    }
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    if (this.resizeObserver_) {
      this.resizeObserver_.disconnect();
      this.resizeObserver_ = null;
    }
    this.eventTracker_.removeAll();
    this.searchboxListenerIds_.forEach(
        id => assert(this.searchboxCallbackRouter_.removeListener(id)));
    this.searchboxListenerIds_ = [];
  }

  // Must have `$` access in updated to avoid violating Lit contract since
  // since `willUpdate` runs before `render`, which will cause `$`
  // to not be populated yet.
  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    if (changedProperties.has('isZeroState')) {
      if (this.isZeroState && !this.isSidePanel) {
        // Get zero state autocomplete matches. In the side panel, we wait for
        // an update about whether an auto-chip will be added before querying
        // autocomplete.
        this.$.composebox.queryAutocomplete(/*clearMatches=*/ false);
      }
    }
  }

  protected get showSuggestions_() {
    return this.isZeroState;
  }

  get showLensButton_() {
    // Lens should be hidden in the side panel if deep search is enabled.
    return this.isSidePanel && this.activeToolMode_ !== ToolMode.kDeepSearch;
  }

  protected getInputPlaceholder_() {
    return this.maybeShowOverlayHintText ?
        loadTimeData.getString('composeboxHintTextLensOverlay') :
        '';
  }

  // Called when cr-composebox suggestion activity link should be
  // shown or hidden. That is calculated based on results and
  // `showDropdown_`.
  protected onShowSuggestionActivityLink_(e: CustomEvent<boolean>) {
    this.showSuggestionsActivityLink_ = e.detail;
  }

  protected onSuggestionsResultReceived_(e: CustomEvent<AutocompleteResult>) {
    this.isLoading_ = false;
    this.zeroStateSuggestions_ = e.detail;
  }

  // Handle keyboard events on the suggestions dropdown.
  protected onDropdownKeydown_(e: KeyboardEvent) {
    if (e.key === 'Enter') {
      this.navigateToMatch_(this.selectedMatchIndex_);
      e.preventDefault();
      e.stopPropagation();
    }
  }

  private updateSelection_(index: number) {
    this.selectedMatchIndex_ = index;
  }

  protected onMatchFocusin_(e: CustomEvent<{index: number}>) {
    this.updateSelection_(e.detail.index);
  }

  private navigateToMatch_(index: number) {
    const match = this.zeroStateSuggestions_?.matches[index];

    if (match) {
      this.searchboxHandler_.openAutocompleteMatch(
          /*line=*/ index,
          /*url=*/ match.destinationUrl,
          /*areMatchesShowing=*/ true,
          /*mouseButton=*/ 0,
          /*altKey=*/ false,
          /*ctrlKey=*/ false,
          /*metaKey=*/ false,
          /*shiftKey=*/ false);
    }
    this.clearInputAndFocus(/* querySubmitted= */ true);
    this.selectedMatchIndex_ = -1;
  }

  clearInputAndFocus(querySubmitted: boolean = false): void {
    // Clear text from composebox and focus.
    this.$.composebox.clearAllInputs(
        querySubmitted, /* shouldBlockAutoSuggestedTabs= */ false);
    this.$.composebox.focusInput();
    this.$.composebox.clearAutocompleteMatches();
  }

  async startExpandAnimation() {
    const composebox = this.$.composebox;
    composebox.animationState = GlowAnimationState.NONE;
    await composebox.updateComplete;
    // Force a reflow to ensure the animation restarts.
    composebox.offsetHeight;
    composebox.animationState = GlowAnimationState.EXPANDING;
  }

  protected handleImageUpload_() {
    this.pageHandler_.handleFileUpload(true);
  }

  protected handleFileUpload_() {
    this.pageHandler_.handleFileUpload(false);
  }

  injectInput(
      title: string, thumbnail: string, fileToken: UnguessableToken,
      supportsUnimodal: boolean) {
    this.$.composebox.injectInput(
        title, thumbnail, fileToken, supportsUnimodal);
  }

  injectInputWithIcon(
      title: string, iconId: IconType, fileToken: UnguessableToken,
      supportsUnimodal: boolean) {
    this.$.composebox.injectInput(
        title, '', fileToken, supportsUnimodal,
        ICON_TYPE_TO_NAME[iconId as number] ?? 'unspecified');
  }

  deleteFile(fileToken: UnguessableToken) {
    this.$.composebox.deleteFile(fileToken);
  }

  getComposebox() {
    return this.$.composebox;
  }

  get isComposeboxFocusedForTesting() {
    return this.isComposeboxFocused_;
  }

  get composeboxHeightForTesting() {
    return this.composeboxHeight_;
  }

  get resizeObserverForTesting() {
    return this.resizeObserver_;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'contextual-tasks-composebox': ContextualTasksComposeboxElement;
  }
}

customElements.define(
    ContextualTasksComposeboxElement.is, ContextualTasksComposeboxElement);
