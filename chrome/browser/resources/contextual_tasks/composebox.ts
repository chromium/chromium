// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_components/composebox/composebox_dropdown.js';
import '//resources/cr_components/composebox/composebox.js';
import '//resources/cr_components/localized_link/localized_link.js';
import './onboarding_tooltip.js';

import type {ComposeboxElement} from '//resources/cr_components/composebox/composebox.js';
import type {PageHandlerRemote} from '//resources/cr_components/composebox/composebox.mojom-webui.js';
import type {ComposeboxDropdownElement} from '//resources/cr_components/composebox/composebox_dropdown.js';
import {ComposeboxProxyImpl, createAutocompleteMatch} from '//resources/cr_components/composebox/composebox_proxy.js';
import {GlowAnimationState} from '//resources/cr_components/search/constants.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {AutocompleteMatch, AutocompleteResult, PageCallbackRouter as SearchboxPageCallbackRouter} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {ToolMode} from '//resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import type {UnguessableToken} from '//resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './composebox.css.js';
import {getHtml} from './composebox.html.js';
import {VoiceSearchState} from './constants.js';
import type {ContextualTasksOnboardingTooltipElement} from './onboarding_tooltip.js';

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
    onboardingTooltip: ContextualTasksOnboardingTooltipElement,
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
      showOnboardingTooltip_: {
        type: Boolean,
        value: loadTimeData.getBoolean('showOnboardingTooltip'),
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
  protected accessor showOnboardingTooltip_: boolean =
      loadTimeData.getBoolean('showOnboardingTooltip');
  protected accessor activeToolMode_: ToolMode = ToolMode.kUnspecified;
  protected accessor showSuggestionsActivityLink_: boolean = false;
  private eventTracker_: EventTracker = new EventTracker();
  private pageHandler_: PageHandlerRemote;
  private searchboxCallbackRouter_: SearchboxPageCallbackRouter;
  private searchboxListenerIds_: number[] = [];
  private onboardingTooltipIsVisible_: boolean = false;
  private numberOfTimesTooltipShown_: number = 0;
  private readonly maximumTimesTooltipShown_: number = loadTimeData.getInteger(
      'composeboxShowOnboardingTooltipSessionImpressionCap');
  private isOnboardingTooltipDismissCountBelowCap_: boolean =
      loadTimeData.getBoolean('isOnboardingTooltipDismissCountBelowCap');
  private userDismissedTooltip_: boolean = false;
  private resizeObserver_: ResizeObserver|null = null;
  private tooltipImpressionTimer_: number|null = null;
  private readonly tooltipImpressionDelay_: number =
      loadTimeData.getInteger('composeboxShowOnboardingTooltipImpressionDelay');

  constructor() {
    super();
    this.pageHandler_ = ComposeboxProxyImpl.getInstance().handler;
    this.searchboxCallbackRouter_ =
        ComposeboxProxyImpl.getInstance().searchboxCallbackRouter;
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
              this.updateTooltipVisibility_();
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
            this.updateTooltipVisibility_();
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
      // Initial check.
      this.updateTooltipVisibility_();
      this.activeToolMode_ = composebox.activeToolMode;

      this.resizeObserver_ = new ResizeObserver(() => {
        this.composeboxHeight_ = composebox.offsetHeight;
        this.fire('composebox-height-update', {height: this.composeboxHeight_});
      });
      this.resizeObserver_.observe(composebox);
    }
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.clearTooltipImpressionTimer_();
    this.stopObservingResize_();
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
    //Lens should be hidden in the side panel if deep search is enabled.
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

  private updateTooltipVisibility_() {
    if (!loadTimeData.getBoolean('showOnboardingTooltip')) {
      return;
    }

    const tooltip = this.$.onboardingTooltip;
    if (!tooltip) {
      return;
    }

    if (this.onboardingTooltipIsVisible_ &&
        !this.$.composebox.getHasAutomaticActiveTabChipToken()) {
      tooltip.hide();
      this.onboardingTooltipIsVisible_ = false;
      this.stopObservingResize_();
      // Clear the timer if the tooltip is hidden. This will prevent it being
      // count as an impression if the chip only showed up briefly.
      this.clearTooltipImpressionTimer_();
    } else if (this.$.composebox.getHasAutomaticActiveTabChipToken()) {
      const target = this.$.composebox.getAutomaticActiveTabChipElement();
      if (target) {
        tooltip.target = target;
      }

      if (this.onboardingTooltipIsVisible_) {
        tooltip.updatePosition();
      } else if (this.shouldShowOnboardingTooltip()) {
        tooltip.show();
        this.startObservingResize_(target);
        this.onboardingTooltipIsVisible_ = true;

        // Start the impression timer if the tooltip is newly shown.
        this.tooltipImpressionTimer_ = setTimeout(() => {
          // If the timer is not cleared, that means the delay passed since the
          // tooltip was shown. Increment the impression count.
          this.numberOfTimesTooltipShown_++;
          this.tooltipImpressionTimer_ = null;
        }, this.tooltipImpressionDelay_);
      }
    }
    tooltip.shouldShow = this.onboardingTooltipIsVisible_;
  }

  private shouldShowOnboardingTooltip(): boolean {
    return this.showOnboardingTooltip_ &&
        this.numberOfTimesTooltipShown_ < this.maximumTimesTooltipShown_ &&
        this.isOnboardingTooltipDismissCountBelowCap_ &&
        !this.userDismissedTooltip_;
  }

  protected onTooltipDismissed_() {
    this.userDismissedTooltip_ = true;
    this.onboardingTooltipIsVisible_ = false;
    this.stopObservingResize_();
    this.clearTooltipImpressionTimer_();
  }

  protected onSuggestionsResultReceived_(e: CustomEvent<AutocompleteResult>) {
    this.isLoading_ = false;
    this.zeroStateSuggestions_ = e.detail;
  }

  private clearTooltipImpressionTimer_() {
    if (this.tooltipImpressionTimer_) {
      clearTimeout(this.tooltipImpressionTimer_);
      this.tooltipImpressionTimer_ = null;
    }
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

  private startObservingResize_(target: Element|null) {
    if (this.resizeObserver_) {
      this.resizeObserver_.disconnect();
    }
    this.resizeObserver_ = new ResizeObserver(() => {
      const tooltip = this.$.onboardingTooltip;
      if (tooltip && tooltip.target) {
        tooltip.updatePosition();
      }
    });
    this.resizeObserver_.observe(this.$.composebox);
    if (target) {
      this.resizeObserver_.observe(target);
    }
  }

  private stopObservingResize_() {
    if (this.resizeObserver_) {
      this.resizeObserver_.disconnect();
      this.resizeObserver_ = null;
    }
  }

  injectInput(title: string, thumbnail: string, fileToken: UnguessableToken) {
    this.$.composebox.injectInput(title, thumbnail, fileToken);
  }

  deleteFile(fileToken: UnguessableToken) {
    this.$.composebox.deleteFile(fileToken);
  }

  get isComposeboxFocusedForTesting() {
    return this.isComposeboxFocused_;
  }

  get composeboxHeightForTesting() {
    return this.composeboxHeight_;
  }

  get numberOfTimesTooltipShownForTesting() {
    return this.numberOfTimesTooltipShown_;
  }

  set numberOfTimesTooltipShownForTesting(n: number) {
    this.numberOfTimesTooltipShown_ = n;
  }

  set userDismissedTooltipForTesting(dismissed: boolean) {
    this.userDismissedTooltip_ = dismissed;
  }

  updateTooltipVisibilityForTesting() {
    this.updateTooltipVisibility_();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'contextual-tasks-composebox': ContextualTasksComposeboxElement;
  }
}

customElements.define(
    ContextualTasksComposeboxElement.is, ContextualTasksComposeboxElement);
