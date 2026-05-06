// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_components/composebox/composebox_dropdown.js';
import '//resources/cr_components/composebox/composebox.js';
import '//resources/cr_components/localized_link/localized_link.js';

import type {ComposeboxElement} from '//resources/cr_components/composebox/composebox.js';
import type {PageHandlerRemote} from '//resources/cr_components/composebox/composebox.mojom-webui.js';
import {LensOverlayDismissalSource} from '//resources/cr_components/composebox/composebox.mojom-webui.js';
import type {ComposeboxDropdownElement} from '//resources/cr_components/composebox/composebox_dropdown.js';
import {ComposeboxProxyImpl, createAutocompleteMatch} from '//resources/cr_components/composebox/composebox_proxy.js';
import {GlowAnimationState, VoiceSearchState} from '//resources/cr_components/search/constants.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {AutocompleteMatch, AutocompleteResult, PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {InputState} from '//resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import {InputType, ToolMode} from '//resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import type {UnguessableToken} from '//resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {WindowOpenDisposition} from 'chrome://resources/mojo/ui/base/mojom/window_open_disposition.mojom-webui.js';

import {getCss} from './composebox.css.js';
import {getHtml} from './composebox.html.js';
import {IconType} from './contextual_tasks.mojom-webui.js';
import type {InjectedInput, PageHandlerInterface} from './contextual_tasks.mojom-webui.js';
import {BrowserProxyImpl} from './contextual_tasks_browser_proxy.js';

const ICON_TYPE_TO_NAME: {[id: number]: string} = {
  [IconType.kUnspecified]: 'unspecified',
  [IconType.kAdd]: 'add',
  [IconType.kFormatQuoteFilled]: 'quoteFilled',
  [IconType.kImage]: 'image',
  [IconType.kDrivePdf]: 'drivePdf',
  [IconType.kCheck]: 'check',
  [IconType.kInvertedFormatQuoteFilled]: 'invertedQuoteFilled',
};

function recordVoiceSearchAction(voiceSearchState: VoiceSearchState) {
  // Safety return statement in rare case chrome metrics is not available.
  if (!chrome.histograms) {
    return;
  }

  chrome.histograms.recordEnumerationValue(
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
      inNlm: {
        type: Boolean,
        reflect: true,
      },
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
      isOverlayOpenForAimVisualSearch: {
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
      inputState_: {
        type: Object,
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
      inVoiceSearchMode_: {
        type: Boolean,
        reflect: true,
      },
      inToolMode_: {
        type: Boolean,
        reflect: true,
      },
      selectedMatchIndex_: {type: Number},
      enableFileHint_: {type: Boolean},
      lensButtonDisabled_: {type: Boolean},
      isCanvasQuerySubmitted: {type: Boolean},
      caretAnimationsEnabled_: {type: Boolean},
      energyEffectEnabled_: {type: Boolean, reflect: true},
      energyEffectAnimationEnabled_: {type: Boolean, reflect: true},
    };
  }

  accessor enableNativeZeroStateSuggestions: boolean = false;
  accessor inNlm: boolean = false;
  accessor inToolMode_: boolean = false;
  accessor isZeroState: boolean = false;
  accessor isSidePanel: boolean = false;
  accessor isLensOverlayShowing: boolean = false;
  accessor isOverlayOpenForAimVisualSearch: boolean = false;
  accessor inputEnabled: boolean = true;
  accessor isCanvasQuerySubmitted: boolean = false;

  protected accessor zeroStateSuggestions_: AutocompleteResult = {
    input: '',
    suggestionGroupsMap: {},
    matches: Array(5).fill(null).map(() => createGhostMatch()),
    smartComposeInlineHint: null,
    sequenceId: 0,
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
  protected accessor inputState_: InputState|null = null;
  protected accessor showSuggestionsActivityLink_: boolean = false;
  protected accessor inVoiceSearchMode_: boolean = false;
  protected accessor selectedMatchIndex_: number = -1;
  protected accessor enableFileHint_: boolean =
      loadTimeData.getBoolean('enableFileHint');
  protected accessor lensButtonDisabled_: boolean = false;
  protected searchboxHandler_: SearchboxPageHandlerRemote;
  private eventTracker_: EventTracker = new EventTracker();
  private pageHandler_: PageHandlerRemote;
  private contextualTasksHandler_: PageHandlerInterface;
  private searchboxCallbackRouter_: SearchboxPageCallbackRouter;
  private searchboxListenerIds_: number[] = [];
  private shouldSubmitAfterUpload_: boolean = false;
  // Tracks the resize of the composebox to provide height updates.
  private resizeObserver_: ResizeObserver|null = null;
  // Glif animation should trigger when:
  // - One time when the panel loads/opens zero state
  // - A query is submitted through the nextbox
  // Animation should NOT trigger when:
  // - User submits a query into zero state/the window switches to
  //   non-zero state
  // - User clicks on a suggestion
  private forceSkipSubmitGlifAnimation_: boolean = false;
  protected accessor caretAnimationsEnabled_: boolean =
      loadTimeData.getBoolean('caretAnimationEnabled');
  protected accessor energyEffectEnabled_: boolean =
      loadTimeData.getBoolean('energyEffectEnabled');
  // The use of energyEffectEnabled to set energyEffectAnimationEnabled_ is
  // intentional. This is to align the gating properties for energy effects
  // across all surfaces (= Nextbox, Omnibox, and Realbox).
  protected accessor energyEffectAnimationEnabled_: boolean =
      loadTimeData.getBoolean('energyEffectEnabled');

  constructor() {
    super();
    this.pageHandler_ = ComposeboxProxyImpl.getInstance().handler;
    this.contextualTasksHandler_ = BrowserProxyImpl.getInstance().handler;
    this.searchboxCallbackRouter_ =
        ComposeboxProxyImpl.getInstance().searchboxCallbackRouter;
    this.searchboxHandler_ = ComposeboxProxyImpl.getInstance().searchboxHandler;
  }

  override connectedCallback() {
    super.connectedCallback();
    this.searchboxListenerIds_.push(
        this.searchboxCallbackRouter_.onInputStateChanged.addListener(
            (inputState: InputState) => {
              this.inputState_ = inputState;
            }));
    const composebox = this.$.composebox;
    if (composebox) {
      // Do not play the glow animation if opening on a thread.
      if (!this.isZeroState) {
        composebox.animationState = GlowAnimationState.NONE;
      }
      this.eventTracker_.add(
          composebox, 'can-submit-files-and-input-changed',
          (e: CustomEvent<{canSubmitFilesAndInput: boolean}>) => {
            if (e.detail.canSubmitFilesAndInput &&
                this.shouldSubmitAfterUpload_) {
              this.shouldSubmitAfterUpload_ = false;
              composebox.submitQuery();
            }
          });
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
        // Don't play the submit animation when transitioning away from zero
        // state or on match click.
        if (this.isZeroState || this.forceSkipSubmitGlifAnimation_) {
          this.forceSkipSubmitGlifAnimation_ = false;
          composebox.animationState = GlowAnimationState.NONE;
          this.clearInputAndFocus(/* querySubmitted= */ true);
          return;
        }
        // Force animation to replay visibly on subsequent submissions.
        composebox.animationState = GlowAnimationState.NONE;
        requestAnimationFrame(() => {
          composebox.animationState = GlowAnimationState.SUBMITTING;
        });
        this.clearInputAndFocus(/* querySubmitted= */ true);
      });
      this.eventTracker_.add(composebox, 'match-click', () => {
        this.forceSkipSubmitGlifAnimation_ = true;
      });
      this.eventTracker_.add(
          composebox, 'carousel-resize', (e: CustomEvent<{height: number}>) => {
            if (e.detail.height !== undefined) {
              composebox.style.setProperty(
                  '--carousel-height', `${e.detail.height}px`);
              this.fire('update-tooltip-visibility');
            }
          });
      this.eventTracker_.add(
          composebox, 'composebox-resize',
          (e: CustomEvent<{height: number}>) => {
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
            this.startVoiceSearch();
            recordVoiceSearchAction(
                VoiceSearchState.VOICE_SEARCH_BUTTON_CLICKED);
          });

      this.eventTracker_.add(
          composebox, 'composebox-voice-search-transcription-success', () => {
            this.endVoiceSearch();
            recordVoiceSearchAction(VoiceSearchState.SUCCESSFUL_TRANSCRIPT);
          });

      this.eventTracker_.add(
          composebox, 'composebox-voice-search-error', () => {
            recordVoiceSearchAction(VoiceSearchState.VOICE_SEARCH_ERROR);
          });
      this.eventTracker_.add(
          composebox, 'composebox-voice-search-error-and-canceled', () => {
            this.endVoiceSearch();
            recordVoiceSearchAction(
                VoiceSearchState.VOICE_SEARCH_ERROR_AND_CANCELED);
          });
      this.eventTracker_.add(
          composebox, 'composebox-voice-search-user-canceled', () => {
            this.endVoiceSearch();
            recordVoiceSearchAction(VoiceSearchState.VOICE_SEARCH_CANCELED);
          });

      this.fire('update-tooltip-visibility', {height: composebox.offsetHeight});

      this.resizeObserver_ = new ResizeObserver(() => {
        this.composeboxHeight_ = composebox.offsetHeight;
        this.fire('composebox-height-update', {height: this.composeboxHeight_});
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
      if (this.isZeroState) {
        this.isCanvasQuerySubmitted = false;
        // Opening zero state triggers animation.
        this.$.composebox.animationState = GlowAnimationState.SUBMITTING;
      }
      if (this.isZeroState && !this.isSidePanel) {
        // Get zero state autocomplete matches. In the side panel, we wait for
        // an update about whether an auto-chip will be added before querying
        // autocomplete.
        this.$.composebox.queryAutocomplete(/*clearMatches=*/ false);
      }
    }
  }

  protected startVoiceSearch() {
    this.inVoiceSearchMode_ = true;
  }

  protected endVoiceSearch() {
    this.inVoiceSearchMode_ = false;
  }

  protected shouldShowSuggestions_() {
    return this.isZeroState && !this.inNlm;
  }

  protected isDropdownNeeded_() {
    return !this.shouldShowSuggestions_();
  }

  protected shouldShowLensButton_() {
    return this.isSidePanel;
  }

  get inputState() {
    return this.inputState_;
  }

  protected getInputPlaceholder_() {
    return this.isOverlayOpenForAimVisualSearch &&
            !this.$.composebox.hasFiles() ?
        loadTimeData.getString('composeboxHintTextLensOverlay') :
        '';
  }

  // Called when cr-composebox suggestion activity link should be
  // shown or hidden. That is calculated based on results and
  // `showDropdown_`.
  protected onShowSuggestionActivityLink_(e: CustomEvent<boolean>) {
    this.showSuggestionsActivityLink_ = e.detail;
  }

  protected onSuggestionActivityLinkClicked_(e: CustomEvent<{event: Event}>) {
    e.detail.event.preventDefault();
    const anchor = e.detail.event.target as HTMLAnchorElement;
    if (anchor && anchor.href) {
      this.contextualTasksHandler_.openUrl(
          anchor.href, WindowOpenDisposition.NEW_FOREGROUND_TAB);
    }
  }

  protected onInputStateChanged_(e: CustomEvent<{inputState: InputState}>) {
    this.inToolMode_ = this.inputState?.activeTool !== ToolMode.kUnspecified;
    const disabledTypes = e.detail.inputState?.disabledInputTypes || [];
    if (disabledTypes.includes(InputType.kLensImage)) {
      this.pageHandler_.closeLensOverlayFromWebUI(
          LensOverlayDismissalSource.kContextualTasksImageUploadsDisabled);
      this.lensButtonDisabled_ = true;
      return;
    }
    this.lensButtonDisabled_ = false;
  }

  protected onSuggestionsResultChanged_(e: CustomEvent<AutocompleteResult>) {
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
    if (e.key === 'ArrowDown' || e.key === 'ArrowUp') {
      e.preventDefault();
      e.stopPropagation();
    }
  }

  // TODO(crbug.com/:494603388): This function definition should be updated: It should be
  // `FocusIn` and use `CustomEvent<number>`. However, this means `composebox_match.ts`
  // and its event needs to be updated, too.
  protected onMatchFocusin_(e: CustomEvent<{index: number}>) {
    this.selectedMatchIndex_ = e.detail.index;
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
    const hadContent = this.$.composebox.input.trim().length > 0 ||
        this.$.composebox.hasFiles();

    // Clear text from composebox and focus.
    this.$.composebox.clearAllInputs(
        querySubmitted, /* shouldBlockAutoSuggestedTabs= */ false);
    this.$.composebox.focusInput();

    // Unconditionally clearing matches wipes out the zero state suggestions
    // when transitioning into or updating the zero state. Therefore, only clear
    // matches if the query was submitted or if the composebox is not in zero
    // state.
    if (querySubmitted || !this.isZeroState) {
      this.$.composebox.clearAutocompleteMatches();
      return;
    }

    // Since the composebox is being rendered in zero state, fetch new zero
    // state suggestions IFF there weren't already suggestions to prevent a
    // flicker.
    if (hadContent) {
      this.$.composebox.queryAutocomplete(/*clearMatches=*/ true);
    }
  }

  setIsLoadingForTesting(isLoading: boolean) {
    this.isLoading_ = isLoading;
  }

  async startExpandAnimation() {
    const composebox = this.$.composebox;
    composebox.animationState = GlowAnimationState.NONE;
    await composebox.updateComplete;
    // Force a reflow to ensure the animation restarts.
    composebox.offsetHeight;
    composebox.animationState = GlowAnimationState.EXPANDING;
  }

  protected onOpenImageUpload_() {
    this.pageHandler_.handleFileUpload(true);
  }

  protected onOpenFileUpload_() {
    this.pageHandler_.handleFileUpload(false);
  }

  async injectInput(input: InjectedInput) {
    if (input.fileToken) {
      const iconId = input.iconId;
      const thumbnail = input.thumbnail ?
          ('chrome://image?url=' + encodeURIComponent(input.thumbnail)) :
          '';
      this.$.composebox.injectInput(
          input.title ?? '', thumbnail, input.fileToken, input.supportsUnimodal,
          iconId !== IconType.kUnspecified ?
              ICON_TYPE_TO_NAME[iconId as number] :
              undefined);
    }

    if (input.queryText !== undefined && input.queryText !== null) {
      this.$.composebox.setInputProgrammatically(
          input.queryText, input.submitAfterInjection);
    }
    // Wait for update so composebox can properly uppdate its input.
    await this.$.composebox.updateComplete;

    if (input.submitAfterInjection) {
      if (!this.$.composebox.canSubmitFilesAndInput) {
        this.shouldSubmitAfterUpload_ = true;
        return;
      }
      this.$.composebox.submitQuery();
    }
  }

  deleteFile(fileToken: UnguessableToken) {
    this.$.composebox.deleteFile(fileToken);
  }

  getComposebox() {
    return this.$.composebox;
  }

  setActiveTool(toolMode: ToolMode) {
    this.searchboxHandler_.setActiveToolMode(toolMode);
  }
  setToolFromUrl(urlString: string) {
    const urlObj = new URL(urlString);
    const inputState = this.inputState;
    if (inputState && inputState.toolConfigs) {
      for (const config of inputState.toolConfigs) {
        if (config.aimUrlParams && config.aimUrlParams.length > 0) {
          const hasParam = config.aimUrlParams.some(p => {
            const value = urlObj.searchParams.get(p.paramKey);
            return value === p.paramValue;
          });
          if (hasParam) {
            if (config.tool === 2 /* ToolMode.kCanvas */) {
              this.isCanvasQuerySubmitted = true;
            }
            if (inputState.activeTool !== config.tool) {
              this.setActiveTool(config.tool);
            }
            break;
          }
        }
      }
    }
  }

  get isComposeboxFocusedForTesting() {
    return this.isComposeboxFocused_;
  }

  get composeboxHeightForTesting() {
    return this.composeboxHeight_;
  }

  get selectedMatchIndexForTesting() {
    return this.selectedMatchIndex_;
  }

  set zeroStateSuggestionsForTesting(val: AutocompleteResult) {
    this.zeroStateSuggestions_ = val;
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
