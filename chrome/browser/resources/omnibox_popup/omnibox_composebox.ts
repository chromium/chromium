// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_components/composebox/composebox_dropdown.js';
import '//resources/cr_components/composebox/composebox_input.js';
import '//resources/cr_components/composebox/composebox_submit.js';
import '//resources/cr_components/composebox/composebox_tool_chip.js';
import '//resources/cr_components/composebox/composebox_voice_search.js';
import '//resources/cr_components/composebox/contextual_entrypoint_button.js';
import '//resources/cr_components/composebox/error_scrim.js';
import '//resources/cr_components/composebox/file_carousel.js';
import '//resources/cr_components/localized_link/localized_link.js';
import '//resources/cr_components/search/animated_glow.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_components/composebox/composebox_lens_search.js';
import '/strings.m.js';

import {ComposeboxFile, getLoadTimeBoolean, hasOnlyAutoAddedTabAttachments, hasOnlyAutoAddedTabs, mapUploadErrorToProcessFilesError, ProcessFilesError} from '//resources/cr_components/composebox/common.js';
import type {TabUpload} from '//resources/cr_components/composebox/common.js';
import type {PageHandlerRemote} from '//resources/cr_components/composebox/composebox.mojom-webui.js';
import type {ComposeboxDropdownElement} from '//resources/cr_components/composebox/composebox_dropdown.js';
import type {ComposeboxInputElement} from '//resources/cr_components/composebox/composebox_input.js';
import {ComposeboxEmbedderMixin, SubmitButtonIconType} from '//resources/cr_components/composebox/composebox_mixin.js';
import {ComposeboxProxyImpl} from '//resources/cr_components/composebox/composebox_proxy.js';
import type {ContextUploadErrorType} from '//resources/cr_components/composebox/composebox_query.mojom-webui.js';
import {ContextUploadStatus} from '//resources/cr_components/composebox/composebox_query.mojom-webui.js';
import type {VoicePermissionPromptState} from '//resources/cr_components/composebox/composebox_voice_search.js';
import type {ContextualEntrypointButtonElement} from '//resources/cr_components/composebox/contextual_entrypoint_button.js';
import {GlowAnimationState} from '//resources/cr_components/search/constants.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {RenderType} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {FileAttachment, PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote, SearchContext, TabAttachment} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {ToolMode} from '//resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import type {UnguessableToken} from '//resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';

import {getCss} from './omnibox_composebox.css.js';
import {getHtml} from './omnibox_composebox.html.js';

const CONTEXTUAL_SUGGESTIONS_POSITION_DURATION_MS = 250;
const CONTEXTUAL_SUGGESTIONS_POSITION_CURVE = 'cubic-bezier(0.2, 0, 0, 1)';
const CONTEXTUAL_SUGGESTIONS_OPACITY_DELAY_MS = 50;
const CONTEXTUAL_SUGGESTIONS_OPACITY_DURATION_MS = 150;

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
      applyContextButtonBackground: {
        reflect: true,
        type: Boolean,
      },
      expanding_: {
        reflect: true,
        type: Boolean,
      },
      animationState: {
        type: String,
        reflect: true,
      },
      // Mixin properties whose defaults are overridden below. Re-listed so
      // Lit wraps this class's accessors; a bare `override accessor` would
      // shadow the mixin's reactive ones.
      searchboxNextEnabled: {type: Boolean, reflect: true},
      composeboxNoFlickerSuggestionsFix: {type: Boolean},
      disableCaretColorAnimation: {type: Boolean, reflect: true},
      energyEffectEnabled: {type: Boolean, reflect: true},
      showMenuOnClick: {type: Boolean},
      shouldShowGhostFiles: {type: Boolean},
      usePecApi: {type: Boolean, reflect: true},
      contextManagementInComposeboxEnabled: {type: Boolean, reflect: true},
      smartComposeEnabled: {type: Boolean, reflect: true},
      submitButtonIconType: {type: String},
      isOblongShape: {type: Boolean},
      webuiOmniboxSimplificationEnabled: {type: Boolean},
      showVoiceSearch: {type: Boolean},
      disableVoiceSearchAnimation: {type: Boolean},
      entrypointName: {type: String, reflect: true},
      enableCarouselScrolling: {type: Boolean},
      askGComposeboxLensChipEnabled_: {type: Boolean},
      // TODO(b/539981162): Consolidate the different lens buttons once
      // behaviors are solidified, and we can tell how they can be merged.
      isLensSearchChipShown_: {type: Boolean},
      isContentSharingEnabled_: {type: Boolean},
      isLensSearchEligible_: {type: Boolean},
      askGComposeboxPlaceholderEnabled_: {type: Boolean},
      baseSearchboxLayoutMode_: {type: String},
      hasPermissionPrompt_: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  accessor entrypointName: string = 'Omnibox';
  accessor applyContextButtonBackground: boolean = false;
  accessor enableCarouselScrolling: boolean = false;
  override accessor animationState: GlowAnimationState =
      GlowAnimationState.NONE;
  // Host-independent configuration; `closeOnEscape` is set by the host page.
  override accessor searchboxNextEnabled: boolean = true;
  override accessor composeboxNoFlickerSuggestionsFix: boolean = true;
  override accessor disableCaretColorAnimation: boolean =
      !loadTimeData.getBoolean('caretAnimationEnabled');
  override accessor energyEffectEnabled: boolean =
      loadTimeData.getBoolean('energyEffectEnabled');
  override accessor showMenuOnClick: boolean = false;
  override accessor shouldShowGhostFiles: boolean = true;
  override accessor usePecApi: boolean =
      loadTimeData.getBoolean('contextualMenuUsePecApi');
  override accessor contextManagementInComposeboxEnabled: boolean =
      loadTimeData.getBoolean('contextManagementInComposeboxEnabled');
  override accessor smartComposeEnabled: boolean =
      loadTimeData.getBoolean('composeboxSmartComposeEnabled');
  override accessor submitButtonIconType: SubmitButtonIconType =
      SubmitButtonIconType.FORWARD;
  override accessor isOblongShape: boolean =
      loadTimeData.getBoolean('contextButtonShapeIsOblong');
  override accessor webuiOmniboxSimplificationEnabled: boolean =
      loadTimeData.getBoolean('webuiOmniboxSimplificationEnabled');
  override accessor showVoiceSearch: boolean = true;
  // Voice search animation is disabled outside of voice coherence.
  override accessor disableVoiceSearchAnimation: boolean =
      !loadTimeData.getBoolean('voiceSearchCoherenceComposeboxesEnabled');
  protected accessor expanding_: boolean = true;
  protected accessor askGComposeboxLensChipEnabled_: boolean =
      getLoadTimeBoolean('askGComposeboxLensChipEnabled', false);
  protected accessor isLensSearchChipShown_: boolean = false;
  protected accessor isContentSharingEnabled_: boolean = false;
  protected accessor isLensSearchEligible_: boolean = false;
  protected accessor askGComposeboxPlaceholderEnabled_: boolean =
      getLoadTimeBoolean('askGComposeboxPlaceholderEnabled', false);
  protected accessor baseSearchboxLayoutMode_: string =
      loadTimeData.getString('searchboxLayoutMode');
  // Set while the voice search permission prompt is showing.
  protected accessor hasPermissionPrompt_: boolean = false;

  protected onLensSearchClick_(e: Event) {
    e.stopPropagation();
    this.getSearchboxHandler().openLensSearch();
  }

  // Reset Lens search eligibility when the popup is hidden. This prevents the
  // cached WebUI from briefly flashing the Lens chip from a previous eligible
  // page when the popup is reopened on an ineligible page (e.g. NTP).
  private onVisibilityChange_ = () => {
    if (document.visibilityState === 'hidden') {
      this.isLensSearchEligible_ = false;
    }
  };

  private pageHandler_: PageHandlerRemote;
  private searchboxCallbackRouter_: SearchboxPageCallbackRouter;
  private searchboxHandler_: SearchboxPageHandlerRemote;
  private listenerIds_: number[] = [];
  /**
   * Whether this session opened the composebox without suggestions and is
   * waiting for contextual ones. Set in `addSearchContext()` and consumed
   * exactly once, on the `showDropdown` when the results come in.
   */
  private awaitingBlockedZeroStateSuggestions_: boolean = false;

  constructor() {
    super();
    this.pageHandler_ = ComposeboxProxyImpl.getInstance().handler;
    this.searchboxCallbackRouter_ =
        ComposeboxProxyImpl.getInstance().searchboxCallbackRouter;
    this.searchboxHandler_ = ComposeboxProxyImpl.getInstance().searchboxHandler;

    this.searchboxLayoutMode = this.computeSearchboxLayoutMode_();

    this.addEventListener(
        'composebox-submit', () => this.onComposeboxSubmit_());
    this.addEventListener(
        'voice-permission-prompt-changed',
        e => this.onVoicePermissionPromptChanged_(
            e as CustomEvent<VoicePermissionPromptState>));
  }

  override connectedCallback() {
    super.connectedCallback();
    this.animationState = GlowAnimationState.EXPANDING;
    this.updateInputPlaceholder();

    this.listenerIds_ = [
      this.searchboxCallbackRouter_.updateContentSharingPolicy.addListener(
          (enabled: boolean) => {
            this.isContentSharingEnabled_ = enabled;
          }),
      this.searchboxCallbackRouter_.updateLensSearchEligibility.addListener(
          (eligible: boolean) => {
            this.isLensSearchEligible_ = eligible;
          }),
    ];

    document.addEventListener('visibilitychange', this.onVisibilityChange_);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    for (const listenerId of this.listenerIds_) {
      this.searchboxCallbackRouter_.removeListener(listenerId);
    }
    this.listenerIds_ = [];
    document.removeEventListener('visibilitychange', this.onVisibilityChange_);
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('inputState') ||
        changedPrivateProperties.has('webuiOmniboxSimplificationEnabled')) {
      const inToolMode = this.inputState?.activeTool !== ToolMode.kUnspecified;
      this.applyContextButtonBackground =
          this.webuiOmniboxSimplificationEnabled && !inToolMode;
    }

    if (changedPrivateProperties.has('input') ||
        changedPrivateProperties.has('attachedContext') ||
        changedPrivateProperties.has('inputState') ||
        changedPrivateProperties.has('isContentSharingEnabled_') ||
        changedPrivateProperties.has('isLensSearchEligible_') ||
        changedPrivateProperties.has('askGComposeboxLensChipEnabled_')) {
      this.isLensSearchChipShown_ = this.askGComposeboxLensChipEnabled_ &&
          this.isContentSharingEnabled_ && this.isLensSearchEligible_ &&
          !this.hasContent(/* ignoreAutoAddedTabs= */ true);
    }

    if (changedPrivateProperties.has('attachedContext') ||
        changedPrivateProperties.has('inputState') ||
        changedPrivateProperties.has('askGComposeboxPlaceholderEnabled_')) {
      this.updateInputPlaceholder();
    }

    if (changedPrivateProperties.has('baseSearchboxLayoutMode_') ||
        changedPrivateProperties.has('hasAllowedInputs')) {
      this.searchboxLayoutMode = this.computeSearchboxLayoutMode_();
    }
  }

  private computeSearchboxLayoutMode_(): string {
    if (this.baseSearchboxLayoutMode_.startsWith('Tall') &&
        !this.hasAllowedInputs) {
      return 'Compact';
    }
    return this.baseSearchboxLayoutMode_;
  }

  override updateInputPlaceholder() {
    const hasAutoAddedTab = hasOnlyAutoAddedTabs(this.attachedContext);
    if (this.askGComposeboxPlaceholderEnabled_ && hasAutoAddedTab &&
        (!this.inputState ||
         this.inputState.activeTool === ToolMode.kUnspecified)) {
      this.inputPlaceholder = this.i18n('askAboutTab');
      return;
    }
    super.updateInputPlaceholder();
  }

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);
    this.focusInput();
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('result')) {
      this.toggleAttribute('has-grid', this.hasGridSuggestionsGroup_());
    }

    // Animate once, when the contextual suggestions arrive. Consuming
    // `awaitingBlockedZeroStateSuggestions_` stops later results from
    // retriggering the animation.
    // `lastQueriedInput` is empty only on the zero state path. A typed query
    // can open the dropdown too and we want to make sure we are not animating
    // that.
    if (this.awaitingBlockedZeroStateSuggestions_ && !this.lastQueriedInput &&
        changedPrivateProperties.has('showDropdown') && this.showDropdown) {
      this.awaitingBlockedZeroStateSuggestions_ = false;
      this.animateContextualSuggestions_();
    }
  }

  /**
   * Plays the load-in animation: the box grows to fit the arriving
   * suggestions, which fade in as it opens.
   */
  private async animateContextualSuggestions_() {
    const dropdown = this.getDropdownElement();
    const box = this.$.composebox;
    const startHeight = box.getBoundingClientRect().height;

    await dropdown.updateComplete;

    const endHeight = box.getBoundingClientRect().height;

    // `fill` is set to backwards to keep the rows transparent until the fade
    // starts.
    dropdown.animate(
        [
          {opacity: 0},
          {opacity: 1},
        ],
        {
          duration: CONTEXTUAL_SUGGESTIONS_OPACITY_DURATION_MS,
          delay: CONTEXTUAL_SUGGESTIONS_OPACITY_DELAY_MS,
          easing: 'linear',
          fill: 'backwards',
        });

    const prefersReducedMotion =
        window.matchMedia('(prefers-reduced-motion: reduce)').matches;
    if (!prefersReducedMotion && startHeight !== endHeight) {
      box.animate(
          [
            {height: `${startHeight}px`},
            {height: `${endHeight}px`},
          ],
          {
            duration: CONTEXTUAL_SUGGESTIONS_POSITION_DURATION_MS,
            easing: CONTEXTUAL_SUGGESTIONS_POSITION_CURVE,
          });
    }
  }

  override deleteFile(uuidToDelete: UnguessableToken, fromUserAction?: boolean):
      ComposeboxFile|null {
    const file = super.deleteFile(uuidToDelete, fromUserAction);
    if (!file) {
      return null;
    }

    this.queryAutocomplete(/* clearMatches= */ true);
    return file;
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
      null {
    return this.shadowRoot?.querySelector<ContextualEntrypointButtonElement>(
               '#contextEntrypoint') ||
        null;
  }

  override shouldShowDivider(): boolean {
    if (!this.shouldHideDropdown() && this.hasGridSuggestionsGroup_()) {
      return true;
    }
    if (this.searchboxLayoutMode === 'TallBottomContext' &&
        !this.showFileCarousel) {
      return false;
    }

    return super.shouldShowDivider();
  }

  override shouldHandleSuggestionFuseboxActions(): boolean {
    return true;
  }

  // ===========================================================================
  // Host API
  // ===========================================================================

  // Force the button to keep its hover background visually while
  // the menu is open, even if the mouse doesn't move out of the button
  // area after clicking.
  setContextMenuOpen(open: boolean) {
    this.getContextEntrypointElement()?.classList.toggle('menu-open', open);
  }

  // ===========================================================================
  // Common event handlers
  // ===========================================================================

  private onComposeboxSubmit_() {
    this.clearAllInputs(
        /* querySubmitted= */ true, /* shouldBlockAutoSuggestedTabs= */ false);
  }

  // Fired from voice search component if minimum height and width are non
  // zero when permission prompt is displayed, or any time when permission
  // prompt hides.
  private onVoicePermissionPromptChanged_(
      e: CustomEvent<VoicePermissionPromptState>) {
    this.hasPermissionPrompt_ = e.detail.isOpened;
    // Only #composebox reads these. The voice search overlay sets its own
    // copy in ComposeboxMixin.onVoicePermissionChanged().
    const composebox = this.$.composebox;
    if (e.detail.isOpened) {  // Permission prompt opened.
      composebox.style.setProperty(
          '--cr_composebox_minimum_height', `${e.detail.height}px`);
      composebox.style.setProperty(
          '--cr_composebox_minimum_width', `${e.detail.width}px`);
    } else {  // Permission prompt closed.
      composebox.style.removeProperty('--cr_composebox_minimum_height');
      composebox.style.removeProperty('--cr_composebox_minimum_width');
    }
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

    const blockZeroState =
        getLoadTimeBoolean('askGBlockAutoTabZeroStateSuggestions', false);
    const hasOnlyAutoAdded =
        context ? hasOnlyAutoAddedTabAttachments(context.attachments) : false;
    const skipImmediateQuery = blockZeroState && hasOnlyAutoAdded;

    // Query for ZPS even if there's no context.
    // Skip immediate query if askGBlockAutoTabZeroStateSuggestions is enabled for
    // auto added tab attachments.
    // This is to prevent historical suggestions from being shown while waiting
    // for contextual suggestions from the uploaded tab during zero state.
    if (this.showZps && !skipImmediateQuery) {
      // Clear the autocomplete matches here, as failure to do so triggers a
      // DCHECK in `ZpsSection::InitMatches()` whenever the user tries to upload
      // a file after having uploaded an invalid file earlier in the session.
      this.queryAutocomplete(/* clearMatches= */ true);
    }

    // Skipping the query is what makes the composebox open empty and expand
    // later, so it is the precondition for the animation. This requires both
    // `askGBlockAutoTabZeroStateSuggestions` to be enabled AND the tab
    // attachment to be auto-added (context-menu-added tabs query immediately
    // and do not animate).
    this.awaitingBlockedZeroStateSuggestions_ = skipImmediateQuery;
  }

  playGlowAnimation() {
    // If |animationState_| were still EXPANDING, this function would have no
    // effect because nothing changes in CSS and therefore animations wouldn't
    // be re-trigered. Resetting it to NONE forces the animation related styles
    // to reset before switching to EXPANDING.
    this.animationState = GlowAnimationState.NONE;
    // Wait for the style change for NONE to commit. This ensures the browser
    // detects a state change when we switch to EXPANDING.

    // If the composebox is not submittable, trigger the animation.
    if (!this.submitEnabled) {
      requestAnimationFrame(() => {
        this.animationState = GlowAnimationState.EXPANDING;
      });
    }
  }

  isExpanded(): boolean {
    return this.expanding_;
  }

  private addFileFromAttachment_(fileAttachment: FileAttachment) {
    const errorType = fileAttachment.errorType ?? null;
    if (errorType) {
      const processFilesError = mapUploadErrorToProcessFilesError(
          errorType as ContextUploadErrorType);
      if (processFilesError !== ProcessFilesError.NONE) {
        this.handleProcessFilesError(processFilesError);
        if (!super.deleteFile(fileAttachment.uuid)) {
          this.getSearchboxHandler().deleteContext(
              fileAttachment.uuid, /*fromAutomaticChip=*/ false);
        }
        return;
      }
    }

    const pendingStatus =
        this.attachedContext.get(fileAttachment.uuid)?.status;
    const composeboxFile = ComposeboxFile.createFromFile(
        fileAttachment.uuid,
        {name: fileAttachment.name, type: fileAttachment.mimeType},
        pendingStatus ?? ContextUploadStatus.kNotUploaded, {
          dataUrl: fileAttachment.imageDataUrl ?? null,
          iconUrl: fileAttachment.iconUrl,
          thumbnailUrl: fileAttachment.thumbnailUrl ?? null,
          supportsUnimodal: true,
        });
    this.onFileContextAdded(composeboxFile);
  }

  private addTabFromAttachment_(tabAttachment: TabAttachment) {
    this.addTabContextHandleCallback({
      tabId: tabAttachment.tabId,
      title: tabAttachment.title,
      url: tabAttachment.url,
      delayUpload: false,
      origin: tabAttachment.source,
    } as TabUpload);
  }

  private hasGridSuggestionsGroup_(): boolean {
    return this.richImageSuggestionsEnabled &&
        (this.result?.matches.some(
             match => this.result?.suggestionGroupsMap[match.suggestionGroupId]
                          ?.renderType === RenderType.kGrid) ??
         false);
  }

  setBaseSearchboxLayoutModeForTesting(mode: string) {
    this.baseSearchboxLayoutMode_ = mode;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-omnibox-composebox': OmniboxComposeboxElement;
  }
}

customElements.define(OmniboxComposeboxElement.is, OmniboxComposeboxElement);
