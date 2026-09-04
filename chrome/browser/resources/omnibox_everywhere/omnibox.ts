// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_components/searchbox/searchbox_dropdown.js';
import '//resources/cr_components/searchbox/searchbox_input.js';
import '//resources/cr_components/searchbox/searchbox_compose_button.js';
import '//resources/cr_components/search/animated_glow.js';
import '//resources/cr_components/composebox/composebox_file_inputs.js';
import '//resources/cr_components/composebox/contextual_entrypoint_and_menu.js';
import './profile_icon.js';

import {ContextType, recordContextAdditionMethod, recordContextualElementClickedMetric, TabSuggestionsState} from '//resources/cr_components/composebox/common.js';
import type {ComposeboxState, ContextualUpload, DriveUpload, TabUpload, TabUploadOrigin} from '//resources/cr_components/composebox/common.js';
import type {ComposeboxFileInputsElement} from '//resources/cr_components/composebox/composebox_file_inputs.js';
import type {ContextualEntrypointAndMenuElement} from '//resources/cr_components/composebox/contextual_entrypoint_and_menu.js';
import {HelpBubbleMixinLit} from '//resources/cr_components/help_bubble/help_bubble_mixin_lit.js';
import {ComposeboxContextAddedMethod, GlowAnimationState} from '//resources/cr_components/search/constants.js';
import {DragAndDropHandler} from '//resources/cr_components/search/drag_drop_handler.js';
import type {DragAndDropHost} from '//resources/cr_components/search/drag_drop_host.js';
import {SearchboxBrowserProxy} from '//resources/cr_components/searchbox/searchbox_browser_proxy.js';
import type {ComposeClickEventDetail} from '//resources/cr_components/searchbox/searchbox_compose_button.js';
import type {SearchboxDropdownElement} from '//resources/cr_components/searchbox/searchbox_dropdown.js';
import type {SearchboxInputElement} from '//resources/cr_components/searchbox/searchbox_input.js';
import type {SearchboxMixinInterface} from '//resources/cr_components/searchbox/searchbox_mixin.js';
import {SearchboxMixin} from '//resources/cr_components/searchbox/searchbox_mixin.js';
import {SearchboxSelectionMixin} from '//resources/cr_components/searchbox/searchbox_selection_mixin.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from '//resources/cr_elements/web_ui_listener_mixin_lit.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import {DriveDisclaimerStatus} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {DriveUploadError, PageCallbackRouter, PageHandlerInterface, TabInfo} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {ModelMode, ToolMode} from '//resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import type {InputState} from '//resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';

import {OmniboxEverywhereBrowserProxyImpl} from './browser_proxy.js';
import {getCss} from './omnibox.css.js';
import {getHtml} from './omnibox.html.js';


export interface OmniboxEverywhereOmniboxElement {
  $: {
    input: SearchboxInputElement,
    inputWrapper: HTMLElement,
    matches: SearchboxDropdownElement,
    fileInputs: ComposeboxFileInputsElement,
  };
}

// Note: Copied from omnibox_popup_searchbox.ts.
const OmniboxEverywhereOmniboxElementBase =
    HelpBubbleMixinLit(SearchboxMixin(SearchboxSelectionMixin(
        I18nMixinLit(WebUiListenerMixinLit(CrLitElement)))));

export class OmniboxEverywhereOmniboxElement extends
    OmniboxEverywhereOmniboxElementBase implements DragAndDropHost,
                                                   SearchboxMixinInterface {
  override get isAimButtonVisible(): boolean {
    return this.composeButtonEnabled;
  }

  override get showContextEntrypoint(): boolean {
    return false;
  }

  static get is() {
    return 'omnibox-everywhere-omnibox';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      virtualFocusEnabled: {
        type: Boolean,
      },
      placeholderText: {
        type: String,
        reflect: true,
        notify: true,
      },
      searchboxChromeRefreshTheming: {
        type: Boolean,
        reflect: true,
      },
      searchboxSteadyStateShadow: {
        type: Boolean,
        reflect: true,
      },
      searchboxIcon_: {type: String},
      searchboxVoiceSearchEnabled_: {
        type: Boolean,
        reflect: true,
      },
      searchboxLensSearchEnabled_: {
        type: Boolean,
        reflect: true,
      },
      animationState: {
        type: String,
        reflect: true,
      },
      inVoiceSearchMode: {
        type: Boolean,
        reflect: true,
      },
      composeButtonEnabled: {type: Boolean, reflect: true},
      profileAvatarUrl_: {type: String},
      isFuseboxEnabled: {type: Boolean, reflect: true},
      hasUserInput_: {type: Boolean},
      ntpRealboxDynamicAiModeButtonEnabled_: {type: Boolean},
      inputState_: {type: Object},
      tabSuggestions_: {type: Array},
      searchboxLayoutMode: {type: String},
      tabSuggestionsState_: {type: Number},
      contextManagementInComposeboxEnabled: {type: Boolean},
      isDraggingFile: {
        reflect: true,
        type: Boolean,
      },
      energyEffectAnimationEnabled_: {type: Boolean},
      fileContextEnabled_: {type: Boolean},
      entrypointName: {type: String},
      isScreenshotMenuOpen: {
        type: Boolean,
        reflect: true,
      },
      isContextMenuOpen: {
        type: Boolean,
        reflect: true,
        attribute: 'is-context-menu-open',
      },
    };
  }

  override accessor virtualFocusEnabled: boolean =
      loadTimeData.valueExists('omniboxEverywhereVirtualFocusNavigation') &&
      loadTimeData.getBoolean('omniboxEverywhereVirtualFocusNavigation');
  accessor placeholderText: string = '';
  accessor entrypointName: string = 'OmniboxEverywhere';
  accessor isDraggingFile: boolean = false;
  accessor isScreenshotMenuOpen: boolean = false;
  accessor isContextMenuOpen: boolean = false;
  protected dragAndDropHandler: DragAndDropHandler;
  protected accessor energyEffectAnimationEnabled_: boolean =
      loadTimeData.getBoolean('energyEffectAnimationEnabled');
  protected accessor fileContextEnabled_: boolean =
      loadTimeData.getBoolean('composeboxContextDragAndDropEnabled');
  accessor searchboxChromeRefreshTheming: boolean =
      loadTimeData.getBoolean('searchboxCr23Theming');
  accessor searchboxSteadyStateShadow: boolean =
      loadTimeData.getBoolean('searchboxCr23SteadyStateShadow');
  accessor contextManagementInComposeboxEnabled: boolean =
      loadTimeData.getBoolean('contextManagementInComposeboxEnabled');
  protected accessor searchboxIcon_: string =
      '//resources/cr_components/searchbox/icons/google_g.svg';
  protected accessor searchboxVoiceSearchEnabled_: boolean =
      loadTimeData.getBoolean('searchboxVoiceSearch');
  protected accessor searchboxLensSearchEnabled_: boolean =
      loadTimeData.getBoolean('searchboxLensSearch');
  accessor animationState: GlowAnimationState = GlowAnimationState.NONE;
  accessor inVoiceSearchMode: boolean = false;
  protected accessor composeButtonEnabled: boolean =
      loadTimeData.getBoolean('searchboxShowComposeEntrypoint');
  protected accessor profileAvatarUrl_: string =
      loadTimeData.getString('profileAvatarUrl');
  protected accessor isFuseboxEnabled: boolean =
      loadTimeData.getBoolean('isFuseboxEnabled');
  protected accessor hasUserInput_: boolean = false;
  protected accessor ntpRealboxDynamicAiModeButtonEnabled_: boolean =
      loadTimeData.getBoolean('ntpRealboxDynamicAiModeButton');
  protected accessor inputState_: InputState|null = null;
  protected accessor tabSuggestions_: TabInfo[] = [];
  protected accessor searchboxLayoutMode: string =
      loadTimeData.getString('searchboxLayoutMode');
  protected accessor tabSuggestionsState_: TabSuggestionsState =
      TabSuggestionsState.NOT_STARTED;

  private pageHandler_: PageHandlerInterface;
  private callbackRouter_: PageCallbackRouter;
  private autocompleteResultChangedListenerId_: number|null = null;
  private inputStateListenerId_: number|null = null;
  private aimPopupEligibilityListenerId_: number|null = null;
  private screenshotMenuClosedListenerId_: number|null = null;

  constructor() {
    super();
    const browserProxy = SearchboxBrowserProxy.getInstance();
    this.pageHandler_ = browserProxy.handler;
    this.callbackRouter_ = browserProxy.callbackRouter;
    this.dragAndDropHandler =
        new DragAndDropHandler(this, this.fileContextEnabled_);
  }

  override connectedCallback() {
    super.connectedCallback();
    this.autocompleteResultChangedListenerId_ =
        this.callbackRouter_.autocompleteResultChanged.addListener(
            this.onAutocompleteResultChanged.bind(this));
    this.inputStateListenerId_ =
        this.callbackRouter_.onInputStateChanged.addListener(
            (inputState: InputState) => {
              this.inputState_ = inputState;
            });
    this.aimPopupEligibilityListenerId_ =
        this.callbackRouter_.updateAimPopupEligibility.addListener(
            (aiModePrefEnabled: boolean) => {
              this.composeButtonEnabled = aiModePrefEnabled &&
                  loadTimeData.getBoolean('searchboxShowComposeEntrypoint');
              this.isFuseboxEnabled = aiModePrefEnabled &&
                  loadTimeData.getBoolean('isFuseboxEnabled');
            });
    this.screenshotMenuClosedListenerId_ =
        this.callbackRouter_.onScreenshotMenuClosed.addListener(() => {
          this.isScreenshotMenuOpen = false;
        });
    this.pageHandler_.getInputState().then((response) => {
      this.inputState_ = response.state;
    });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    if (this.autocompleteResultChangedListenerId_ !== null) {
      this.callbackRouter_.removeListener(
          this.autocompleteResultChangedListenerId_);
      this.autocompleteResultChangedListenerId_ = null;
    }
    if (this.inputStateListenerId_ !== null) {
      this.callbackRouter_.removeListener(this.inputStateListenerId_);
      this.inputStateListenerId_ = null;
    }
    if (this.aimPopupEligibilityListenerId_ !== null) {
      this.callbackRouter_.removeListener(this.aimPopupEligibilityListenerId_);
      this.aimPopupEligibilityListenerId_ = null;
    }
    if (this.screenshotMenuClosedListenerId_ !== null) {
      this.callbackRouter_.removeListener(this.screenshotMenuClosedListenerId_);
      this.screenshotMenuClosedListenerId_ = null;
    }
  }

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);
    this.initialInputScrollHeight = this.$.input.scrollHeight;
    const lensButton =
        this.shadowRoot?.querySelector<HTMLElement>('#lensSearchButton');
    if (lensButton) {
      this.registerHelpBubble(
          'kOmniboxEverywhereLensButtonElementId', lensButton);
    }
  }

  focusInput() {
    this.$.input.focus();
  }

  // Returns the current text value of the Omnibox search input. Used by
  // `OmniboxEverywhereAppElement` to seamlessly migrate user-typed queries into
  // the Composebox input field when transitioning into Composebox mode (e.g.
  // when clicking a tool or attaching a tab from the '+' context menu) so typed
  // text is never lost.
  getInputText(): string {
    return this.$.input?.getInputValue() || '';
  }

  setInputText(text: string) {
    this.$.input.setInputText(text);
    this.hasUserInput_ = !!text.trim();
  }

  getDropTarget() {
    return this;
  }

  addDroppedFiles(files: FileList) {
    this.processFiles_(files, ComposeboxContextAddedMethod.DRAG_AND_DROP);
  }

  //========================================================================
  // SearchboxMixin abstract method implementations
  //========================================================================

  override getInputElement(): SearchboxInputElement {
    return this.$.input;
  }

  override getDropdownElement(): SearchboxDropdownElement {
    return this.$.matches;
  }

  override getWrapperElement(): HTMLElement {
    return this.$.inputWrapper;
  }

  override pageHandler(): PageHandlerInterface {
    return this.pageHandler_;
  }

  // Because Omnibox Everywhere keeps its WebContents alive in the background
  // across hide/show cycles, input text must be explicitly cleared on match
  // navigation/submission so subsequent invocations start with a clean input.
  override navigateToMatch(matchIndex: number, e: KeyboardEvent|MouseEvent) {
    super.navigateToMatch(matchIndex, e);
    this.setInputText('');
  }

  override openCtrlEnterMatch(matchIndex: number) {
    super.openCtrlEnterMatch(matchIndex);
    this.setInputText('');
  }

  override onMatchClick() {
    super.onMatchClick();
    this.setInputText('');
  }

  //========================================================================
  // Event handlers
  //========================================================================

  protected onInputFocusin_() {
    this.pageHandler_.onFocusChanged(true);
  }

  isInputEmpty(): boolean {
    // If this is called before first render, the input element will not exist.
    if (!this.shadowRoot?.querySelector('#input') || !this.$.input) {
      return true;
    }
    return !this.$.input.getInputValue().trim();
  }

  protected showVoiceSearchButton_(): boolean {
    return this.searchboxVoiceSearchEnabled_ && this.isInputEmpty();
  }

  protected showLensSearchButton_(): boolean {
    return this.isFuseboxEnabled && this.searchboxLensSearchEnabled_;
  }

  protected computePlaceholderText_(): string {
    if (this.placeholderText) {
      return this.placeholderText;
    }
    if (this.isFuseboxEnabled) {
      return this.i18n('searchBoxHintAskOrType');
    }
    return this.i18n('searchBoxHint');
  }

  protected onSearchboxInputTextUpdated_(
      e: CustomEvent<{value: string, isComposing: boolean}>) {
    this.hasUserInput_ = !!e.detail.value.trim();
    this.onSearchboxInputTextUpdated(e);
  }

  protected async onVoiceSearchButtonClick_() {
    this.animationState = GlowAnimationState.NONE;
    await this.updateComplete;
    this.animationState = GlowAnimationState.LISTENING;
    this.inVoiceSearchMode = true;
    this.dispatchEvent(
        new Event('open-voice-search', {bubbles: true, composed: true}));
  }

  protected onLensSearchClick_(e: Event) {
    this.notifyHelpBubbleAnchorActivated(
        'kOmniboxEverywhereLensButtonElementId');
    this.isScreenshotMenuOpen = true;
    const anchor = e.currentTarget as HTMLElement;
    const rect = anchor.getBoundingClientRect();
    this.pageHandler_.showScreenshotMenu({
      x: Math.round(rect.left),
      y: Math.round(rect.top),
      width: Math.round(rect.width),
      height: Math.round(rect.height),
    });
  }

  protected async onOpenDriveUpload_() {
    // Check if the user has accepted the Drive disclaimer. This handles
    // the edge case where a user sees the drive option in the menu, but
    // then revokes Drive permissions.
    const {status} = await this.pageHandler().getDriveDisclaimerStatus();
    if (status === DriveDisclaimerStatus.kRestricted) {
      return;
    }

    const {response} = await this.pageHandler().onDriveUploadClicked();

    const driveUploads: DriveUpload[] =
        response.files.map(file => ({
                             token: file.token,
                             mimeType: file.mimeType,
                             fileName: file.fileName,
                             thumbnailUrl: file.thumbnailUrl ?? null,
                             iconUrl: file.iconUrl ?? null,
                           }));

    recordContextualElementClickedMetric(
        this.composeboxSource, 'ClassicPopup', ContextType.DRIVE);

    if (driveUploads.length > 0 || response.error !== null) {
      this.openComposebox_(
          driveUploads, ToolMode.kUnspecified, ModelMode.kUnspecified,
          response.error ?? undefined);
    }
  }

  protected onAddTabContext_(e: CustomEvent<{
    id: number,
    title: string,
    url: Url,
    delayUpload: boolean,
    origin: TabUploadOrigin,
  }>) {
    const tabUpload: TabUpload = {
      tabId: e.detail.id,
      title: e.detail.title,
      url: e.detail.url,
      delayUpload: e.detail.delayUpload,
      origin: e.detail.origin,
    };
    recordContextualElementClickedMetric(
        this.composeboxSource, 'ClassicPopup', ContextType.TAB);
    const contextMenu =
        this.shadowRoot?.querySelector<ContextualEntrypointAndMenuElement>(
            '#context');
    contextMenu?.closeMenu();
    this.openComposebox_([tabUpload]);
  }

  protected onFileChange_(e: CustomEvent<{files: FileList}>) {
    this.processFiles_(
        e.detail.files, ComposeboxContextAddedMethod.CONTEXT_MENU);
  }

  protected onSearchboxInputFilesPasted_(e: CustomEvent<{files: FileList}>) {
    this.processFiles_(e.detail.files, ComposeboxContextAddedMethod.COPY_PASTE);
  }

  protected processFiles_(
      files: FileList|null,
      contextAdditionMethod: ComposeboxContextAddedMethod) {
    if (!files || files.length === 0) {
      return;
    }
    recordContextAdditionMethod(contextAdditionMethod, 'OmniboxEverywhere');

    this.openComposebox_(Array.from(files, (file) => ({file})));
  }

  protected openComposebox_(
      uploads: ContextualUpload[] = [], mode: ToolMode = ToolMode.kUnspecified,
      model: ModelMode = ModelMode.kUnspecified, error?: DriveUploadError) {
    this.fire<ComposeboxState>('open-composebox', {
      text: this.$.input.inputElement.value,
      files: uploads,
      mode: mode,
      model: model,
      error: error,
      smartTabSharingActive: false,
    });
    // Clear searchbox input so stale text does not linger behind composebox.
    this.setInputText('');
  }

  protected async openComposeboxWithMode_(mode?: ToolMode, model?: ModelMode) {
    this.animationState = GlowAnimationState.NONE;
    await this.updateComplete;
    this.animationState = GlowAnimationState.LISTENING;
    setTimeout(() => {
      this.openComposebox_([], mode, model);
    }, 300);
  }

  protected onComposeClick_(e: CustomEvent<ComposeClickEventDetail>) {
    this.pageHandler().activateMetricsFunnel('AiModeButton');

    const isSearch = this.selectedMatch?.isSearchType ?? true;
    if (!isSearch) {
      this.setInputText('');
    }
    const queryText = isSearch ? this.$.input.inputElement.value.trim() : '';

    if (queryText) {
      this.pageHandler().notifySessionStarted();
      // TODO(crbug.com/548024751): Add metrics here like normal omnibox and
      // realbox.
      this.pageHandler().submitQuery(
          queryText, e.detail.button, false, /* altKey */
          e.detail.ctrlKey, e.detail.metaKey, e.detail.shiftKey,
          /* isVoiceSearch */ false);
      this.clearAutocompleteMatches();
      this.setInputText('');
    } else {
      this.openComposeboxWithMode_();
    }
  }

  protected onContextMenuEntrypointClick_(e?: CustomEvent<{
    anchorRect?: {x: number, y: number, width: number, height: number},
  }>) {
    this.pageHandler().activateMetricsFunnel('PlusButton');
    this.isContextMenuOpen = true;
    // `anchorRect` is in WebUI viewport coordinates (CSS DIPs). Forward the
    // full bounding box so C++ can translate it to screen coordinates and
    // position the native context menu.
    const rect = e?.detail?.anchorRect ||
        this.shadowRoot?.querySelector('#context')?.getBoundingClientRect();
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

  onContextMenuClosed() {
    this.isContextMenuOpen = false;
    const entrypoint =
        this.shadowRoot?.querySelector<ContextualEntrypointAndMenuElement>(
            '#context');
    entrypoint?.closeMenu();
  }

  protected async refreshTabSuggestions_(forceRefresh: boolean = false) {
    if (this.tabSuggestionsState_ === TabSuggestionsState.LOADING ||
        (this.tabSuggestionsState_ === TabSuggestionsState.LOADED &&
         !forceRefresh)) {
      return;
    }
    this.tabSuggestionsState_ = TabSuggestionsState.LOADING;
    try {
      const {tabs} = await this.pageHandler_.getRecentTabs();
      this.tabSuggestions_ = [...tabs];
      this.tabSuggestionsState_ = TabSuggestionsState.LOADED;
    } finally {
      if (this.tabSuggestionsState_ === TabSuggestionsState.LOADING) {
        this.tabSuggestionsState_ = TabSuggestionsState.NOT_STARTED;
      }
    }
  }

  protected onContextMenuOpened_() {
    this.refreshTabSuggestions_(/*forceRefresh=*/ true);
  }

  protected onContextMenuClosed_() {
    this.tabSuggestionsState_ = TabSuggestionsState.NOT_STARTED;
  }

  override onInputWrapperFocusout(e: FocusEvent) {
    if (this.isContextMenuOpen) {
      return;
    }
    super.onInputWrapperFocusout(e);
  }

  protected onRequestTabSuggestionsLoad() {
    this.refreshTabSuggestions_(/*forceRefresh=*/ true);
  }

  protected onToolClick_(e: CustomEvent<{toolMode: ToolMode}>) {
    this.openComposeboxWithMode_(e.detail.toolMode);
  }

  protected onDeepSearchClick_() {
    this.openComposeboxWithMode_(ToolMode.kDeepSearch);
  }

  protected onCreateImageClick_() {
    this.openComposeboxWithMode_(ToolMode.kImageGen);
  }

  protected onModelClick_(e: CustomEvent<{model: ModelMode}>) {
    this.openComposeboxWithMode_(ToolMode.kUnspecified, e.detail.model);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'omnibox-everywhere-omnibox': OmniboxEverywhereOmniboxElement;
  }
}

customElements.define(
    OmniboxEverywhereOmniboxElement.is, OmniboxEverywhereOmniboxElement);
