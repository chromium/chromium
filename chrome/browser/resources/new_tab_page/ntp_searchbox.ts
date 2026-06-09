// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_components/composebox/composebox_file_inputs.js';
import '//resources/cr_components/composebox/contextual_entrypoint_and_menu.js';
import '//resources/cr_components/searchbox/searchbox_compose_button.js';
import '//resources/cr_components/searchbox/searchbox_dropdown.js';
import '//resources/cr_components/search/animated_glow.js';
import '//resources/cr_components/searchbox/searchbox_input.js';

import type {ComposeboxState, ContextualUpload, DriveUpload, TabUpload, TabUploadOrigin} from '//resources/cr_components/composebox/common.js';
import {ContextType, GlifAnimationState, recordContextAdditionMethod, recordContextualElementClickedMetric, recordInputTypeShown, recordModelModeSelection, recordModelModeShown, recordToolModeSelection, recordToolModeShown} from '//resources/cr_components/composebox/common.js';
import type {ContextualEntrypointAndMenuElement} from '//resources/cr_components/composebox/contextual_entrypoint_and_menu.js';
import {ComposeboxContextAddedMethod, GlowAnimationState} from '//resources/cr_components/search/constants.js';
import {DragAndDropHandler} from '//resources/cr_components/search/drag_drop_handler.js';
import type {DragAndDropHost} from '//resources/cr_components/search/drag_drop_host.js';
import {PlaceholderTextCycler} from '//resources/cr_components/searchbox/placeholder_text_cycler.js';
import {SearchboxBrowserProxy} from '//resources/cr_components/searchbox/searchbox_browser_proxy.js';
import type {SearchboxDropdownElement} from '//resources/cr_components/searchbox/searchbox_dropdown.js';
import type {SearchboxInputElement} from '//resources/cr_components/searchbox/searchbox_input.js';
import {SearchboxMixin} from '//resources/cr_components/searchbox/searchbox_mixin.js';
import type {SearchboxMixinInterface} from '//resources/cr_components/searchbox/searchbox_mixin.js';
import {waitForLazyRender} from '//resources/cr_components/searchbox/utils.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from '//resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {DriveUploadError, PageCallbackRouter, PageHandlerInterface, TabInfo} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {InputState} from '//resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import {InputType, ModelMode, ToolMode} from '//resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';

import {getCss} from './ntp_searchbox.css.js';
import {getHtml} from './ntp_searchbox.html.js';



interface ClickEventDetail {
  button: number;
  ctrlKey: boolean;
  metaKey: boolean;
  shiftKey: boolean;
}

export interface NtpSearchboxElement {
  $: {
    input: SearchboxInputElement,
    inputWrapper: HTMLElement,
  };
}

const NtpSearchboxElementBase =
    SearchboxMixin(I18nMixinLit(WebUiListenerMixinLit(CrLitElement)));

/** A search box for the NTP that behaves like the Omnibox. */
export class NtpSearchboxElement extends NtpSearchboxElementBase implements
    DragAndDropHost, SearchboxMixinInterface {
  static get is() {
    return 'ntp-searchbox';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      //========================================================================
      // Public properties
      //========================================================================
      ntpRealboxNextEnabled: {
        type: Boolean,
        reflect: true,
      },

      composeboxEnabled: {type: Boolean},

      composeButtonEnabled: {type: Boolean},

      cyclingPlaceholders: {type: Boolean},

      isDraggingFile: {
        reflect: true,
        type: Boolean,
      },

      contextMenuGlifAnimationState: {
        type: String,
        reflect: true,
      },

      animationState: {
        reflect: true,
        type: String,
      },

      colorSourceIsBaseline: {
        type: Boolean,
        reflect: true,
      },

      /** Whether the theme is dark. */
      isDark: {
        type: Boolean,
        reflect: true,
      },

      searchboxLayoutMode: {
        type: String,
        reflect: true,
      },

      /**
       * Whether the secondary side can be shown based on the feature state and
       * the width available to the dropdown.
       */
      canShowSecondarySide: {
        type: Boolean,
        reflect: true,
      },

      inVoiceSearchMode: {
        type: Boolean,
        reflect: true,
      },

      /**
       * Whether the secondary side was at any point available to be shown.
       */
      hadSecondarySide: {
        type: Boolean,
        reflect: true,
        notify: true,
      },

      /*
       * Whether the secondary side is currently available to be shown.
       */
      hasSecondarySide: {
        type: Boolean,
        reflect: true,
      },

      searchboxChromeRefreshTheming: {
        type: Boolean,
        reflect: true,
      },

      searchboxSteadyStateShadow: {
        type: Boolean,
        reflect: true,
      },

      placeholderText: {
        type: String,
        reflect: true,
        notify: true,
      },

      hasVoiceSearchError: {type: Boolean},

      isListening: {type: Boolean},

      //========================================================================
      // Protected properties
      //========================================================================
      tabSuggestions_: {type: Array},
      inputState_: {type: Object},
      recentTabId_: {type: Number},

      /** Searchbox default icon (i.e., Google G icon or the search loupe). */
      searchboxIcon_: {type: String},

      /** Whether the voice search icon should be visible in the searchbox. */
      searchboxVoiceSearchEnabled_: {
        type: Boolean,
        reflect: true,
      },

      /** Whether the Google Lens icon should be visible in the searchbox. */
      searchboxLensSearchEnabled_: {
        type: Boolean,
        reflect: true,
      },

      useWebkitSearchIcons_: {
        type: Boolean,
        reflect: true,
      },
      energyEffectAnimationEnabled: {type: Boolean},
    };
  }

  accessor ntpRealboxNextEnabled: boolean = false;
  accessor energyEffectAnimationEnabled: boolean = false;
  accessor composeboxEnabled: boolean = false;
  accessor composeButtonEnabled: boolean = false;
  accessor cyclingPlaceholders: boolean = false;
  accessor isDraggingFile: boolean = false;
  accessor contextMenuGlifAnimationState: GlifAnimationState =
      GlifAnimationState.INELIGIBLE;
  accessor animationState: GlowAnimationState = GlowAnimationState.NONE;
  accessor colorSourceIsBaseline: boolean = false;
  accessor isDark: boolean = false;
  accessor searchboxLayoutMode: string = '';
  accessor canShowSecondarySide: boolean = false;
  accessor hadSecondarySide: boolean = false;
  accessor hasSecondarySide: boolean = false;
  accessor searchboxChromeRefreshTheming: boolean =
      loadTimeData.getBoolean('searchboxCr23Theming');
  accessor searchboxSteadyStateShadow: boolean =
      loadTimeData.getBoolean('searchboxCr23SteadyStateShadow');
  accessor placeholderText: string = '';
  accessor recentTabId_: number|null = null;

  accessor inVoiceSearchMode: boolean = false;
  // If voice search error scrim is showing:
  accessor hasVoiceSearchError: boolean = false;
  // Voice search is listening if there is no error and voice search overlay
  // is open (and active).
  accessor isListening: boolean = false;
  protected accessor tabSuggestions_: TabInfo[] = [];
  protected accessor inputState_: InputState|null = null;
  protected accessor searchboxIcon_: string =
      loadTimeData.getString('searchboxDefaultIcon');
  protected accessor searchboxVoiceSearchEnabled_: boolean =
      loadTimeData.getBoolean('searchboxVoiceSearch');
  protected accessor searchboxLensSearchEnabled_: boolean =
      loadTimeData.getBoolean('searchboxLensSearch');
  protected accessor useWebkitSearchIcons_: boolean = false;
  protected dragAndDropHandler: DragAndDropHandler|null = null;
  protected callbackRouter_: PageCallbackRouter;

  private placeholderCycler_: PlaceholderTextCycler | null = null;
  private dragAndDropEnabled_: boolean =
      loadTimeData.getBoolean('composeboxContextDragAndDropEnabled');
  private onTabStripChangedListenerId_: number|null = null;
  private contextMenuOpened_: boolean = false;
  private pageHandler_: PageHandlerInterface;
  private autocompleteResultChangedListenerId_: number|null = null;

  constructor() {
    performance.mark('searchbox-creation-start');
    super();

    this.pageHandler_ = SearchboxBrowserProxy.getInstance().handler;
    this.callbackRouter_ = SearchboxBrowserProxy.getInstance().callbackRouter;
  }

  override async connectedCallback() {
    super.connectedCallback();
    this.autocompleteResultChangedListenerId_ =
        this.callbackRouter_.autocompleteResultChanged.addListener(
            this.onAutocompleteResultChanged.bind(this));

    if (this.ntpRealboxNextEnabled) {
      this.dragAndDropHandler =
          new DragAndDropHandler(this, this.dragAndDropEnabled_);
    }
    this.onTabStripChangedListenerId_ =
        this.callbackRouter_.onTabStripChanged.addListener(
            this.refreshTabSuggestions_.bind(this));
    this.inputState_ =
        (await this.pageHandler().getInputState())?.state ?? null;
    if (this.inputState_) {
      this.inputState_.activeModel = ModelMode.kUnspecified;
    }
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    if (this.autocompleteResultChangedListenerId_ !== null) {
      this.callbackRouter_.removeListener(
          this.autocompleteResultChangedListenerId_);
      this.autocompleteResultChangedListenerId_ = null;
    }

    this.placeholderCycler_?.stop();
    if (this.onTabStripChangedListenerId_ !== null) {
      this.callbackRouter_.removeListener(this.onTabStripChangedListenerId_);
      this.onTabStripChangedListenerId_ = null;
    }
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('composeButtonEnabled') ||
        changedProperties.has('searchboxChromeRefreshTheming') ||
        changedProperties.has('colorSourceIsBaseline')) {
      this.useWebkitSearchIcons_ = this.composeButtonEnabled ||
          (this.searchboxChromeRefreshTheming && !this.colorSourceIsBaseline);
    }

    if (changedProperties.has('inVoiceSearchMode') ||
        changedProperties.has('hasVoiceSearchError')) {
      this.isListening = this.inVoiceSearchMode && !this.hasVoiceSearchError;
    }
  }

  override firstUpdated() {
    // After crbug.com/502367598, there is no super.firstUpdated() call, the new
    // base chain does not override firstUpdated().
    if (performance.getEntriesByName('realbox-creation-start').length > 0) {
      performance.measure('realbox-creation', 'realbox-creation-start');
    }
    this.initialInputScrollHeight = this.$.input.scrollHeight;

    if (this.cyclingPlaceholders) {
      waitForLazyRender().then(async () => {
        const {config} = await this.pageHandler().getPlaceholderConfig();
        const texts = config.texts;
        if (texts.length < 2) {
          // Need at least 2 placeholders to cycle. If fewer, disable cycling
          // and let the static placeholder text show instead.
          return;
        }
        this.placeholderText = texts[0]!;
        this.placeholderCycler_ = new PlaceholderTextCycler(
            this.$.input.inputElement, texts,
            Number(config.changeTextAnimationInterval.microseconds / 1000n),
            Number(config.fadeTextAnimationDuration.microseconds / 1000n));
        this.placeholderCycler_.start();
      });
    }
  }

  protected shouldShowVoiceLens_(isEnabled: boolean): boolean {
    return isEnabled && this.isInputEmpty() &&
        !(this.dropdownIsVisible && this.composeButtonEnabled);
  }

  override handleKeyNavigation(e: KeyboardEvent) {
    if (this.composeButtonEnabled && e.key === 'Tab' &&
        this.$.input?.lastInput()?.inline &&
        this.$.input === this.shadowRoot.activeElement) {
      if (e.shiftKey) {
        this.$.input.setInput({inline: ''});
        return;
      }

      const newText =
          this.$.input.lastInput()!.text + this.$.input.lastInput()!.inline;
      this.$.input.setInput({
        text: newText,
        inline: '',
        moveCursorToEnd: true,
      });
      this.queryAutocomplete(newText, false);
      e.preventDefault();
      return;
    }

    super.handleKeyNavigation(e);
  }

  protected onInputFocusin_() {
    this.pageHandler_.onFocusChanged(true);
    this.placeholderCycler_?.stop();
  }

  override onInputWrapperFocusout(e: FocusEvent) {
    super.onInputWrapperFocusout(e);
    this.placeholderCycler_?.start();
  }

  //============================================================================
  // Mixin abstract method implementations
  //============================================================================

  override getInputElement(): SearchboxInputElement {
    return this.$.input;
  }

  override getDropdownElement(): SearchboxDropdownElement {
    const matches =
        this.shadowRoot.querySelector<SearchboxDropdownElement>('#matches');
    assert(matches);
    return matches;
  }

  override getWrapperElement(): HTMLElement {
    return this.$.inputWrapper;
  }

  override pageHandler(): PageHandlerInterface {
    return this.pageHandler_;
  }

  //============================================================================
  // Public API (ported from SearchboxElement)
  //============================================================================

  isInputEmpty(): boolean {
    // If this is called before first render, the input element will not exist.
    if (!this.shadowRoot?.querySelector('#input') || !this.$.input ||
        !this.$.input.lastInput()) {
      return true;
    }
    return !this.$.input.lastInput()!.text.trim();
  }

  queryInputAutocomplete() {
    this.queryAutocomplete(this.$.input.inputElement.value, false);
  }

  setInputText(text: string) {
    this.$.input.setInputText(text);
  }

  focusInput() {
    this.$.input.focus();
  }

  blurInput() {
    this.$.input.blur();
  }

  selectAll() {
    this.$.input.select();
  }

  getDropTarget() {
    return this;
  }

  addDroppedFiles(files: FileList) {
    this.processFiles_(files, ComposeboxContextAddedMethod.DRAG_AND_DROP);
  }

  //============================================================================
  // Event handlers
  //============================================================================

  // Perform animation work in this file (`ntp_searchbox.ts`) since this
  // file owns the animation state while `new_tab_page/app.ts` owns the
  // voice component and its initialization.
  async onVoiceSearchClick() {
    this.animationState = GlowAnimationState.NONE;
    await this.updateComplete;
    this.animationState = GlowAnimationState.LISTENING;
    this.inVoiceSearchMode = true;
    // `new_tab_page/app.ts` controls the voice component and when it will
    // start. `new_tab_page/app.ts` sets `inVoiceSearchMode` to `true`
    // with `new_tab_page/app.ts`'s equivalent state
    // `showVoiceSearchOverlay`.
    this.dispatchEvent(new Event('open-voice-search'));
  }

  protected onFileChange_(e: CustomEvent<{files: FileList}>) {
    this.processFiles_(
        e.detail.files, ComposeboxContextAddedMethod.CONTEXT_MENU);
  }

  protected onAddTabContext_(e: CustomEvent<{
    id: number,
    title: string,
    url: Url,
    delayUpload: boolean,
    origin: TabUploadOrigin,
  }>) {
    const attachment: TabUpload = {
      tabId: e.detail.id,
      url: e.detail.url,
      title: e.detail.title,
      delayUpload: e.detail.delayUpload,
      origin: e.detail.origin,
    };
    recordContextualElementClickedMetric(
        this.composeboxSource, 'ClassicPopup', ContextType.TAB);
    this.openComposebox_([attachment]);
  }

  protected async refreshTabSuggestions_(forceRefresh: boolean = false) {
    // Only refresh tab suggestions if the context menu is opened.
    const requiresRefresh = forceRefresh || this.contextMenuOpened_;
    if (!requiresRefresh) {
      return;
    }
    const {tabs} = await this.pageHandler().getRecentTabs();
    this.recentTabId_ = tabs[0]?.tabId ?? null;
    this.tabSuggestions_ = [...tabs];

    if (this.contextMenuOpened_ && this.inputState_) {
      const {allowedInputTypes, disabledInputTypes} = this.inputState_;
      if (allowedInputTypes.includes(InputType.kBrowserTab) &&
          !disabledInputTypes.includes(InputType.kBrowserTab) &&
          this.tabSuggestions_.length > 0) {
        recordInputTypeShown(
            InputType.kBrowserTab, this.composeboxSource, 'ClassicPopup');
      }
    }
  }

  protected async onGetTabPreview_(e: CustomEvent<{
    tabId: number,
    onPreviewFetched: (previewDataUrl: string) => void,
  }>) {
    const {previewDataUrl} =
        await this.pageHandler().getTabPreview(e.detail.tabId);
    e.detail.onPreviewFetched(previewDataUrl || '');
  }

  protected onContextMenuClosed_() {
    this.contextMenuOpened_ = false;
    this.blur();
  }

  protected onContextMenuOpened_() {
    this.contextMenuOpened_ = true;
    this.refreshTabSuggestions_(/*forceRefresh=*/ true);

    if (this.inputState_) {
      const {allowedInputTypes, disabledInputTypes} = this.inputState_;
      allowedInputTypes.forEach((inputType: InputType) => {
        // The `kBrowserTab` InputType requires special metrics handling as part
        // of `refreshTabSuggestions_()`.
        if (inputType !== InputType.kBrowserTab &&
            !disabledInputTypes.includes(inputType)) {
          recordInputTypeShown(
              inputType, this.composeboxSource, 'ClassicPopup');
        }
      });

      const {allowedTools, disabledTools} = this.inputState_;
      allowedTools.forEach((tool: ToolMode) => {
        if (!disabledTools.includes(tool)) {
          recordToolModeShown(tool, this.composeboxSource, 'ClassicPopup');
        }
      });

      const {allowedModels, disabledModels} = this.inputState_;
      allowedModels.forEach((model: ModelMode) => {
        if (!disabledModels.includes(model)) {
          recordModelModeShown(model, this.composeboxSource, 'ClassicPopup');
        }
      });
    }
  }

  protected async onOpenDriveUpload_() {
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

  protected onContextMenuEntrypointClick_() {
    this.pageHandler().activateMetricsFunnel('PlusButton');
  }

  protected onToolClick_(e: CustomEvent<{toolMode: ToolMode}>) {
    this.openComposebox_([], e.detail.toolMode);
  }

  protected onDeepSearchClick_() {
    this.openComposebox_([], ToolMode.kDeepSearch);
  }

  protected onCreateImageClick_() {
    this.openComposebox_([], ToolMode.kImageGen);
  }

  protected onModelClick_(e: CustomEvent<{model: ModelMode}>) {
    this.openComposebox_([], ToolMode.kUnspecified, e.detail.model);
  }

  protected onComposeClick_(e: CustomEvent<ClickEventDetail>) {
    // TODO(crbug.com/463667769): Call submitQuery here since RealboxHandler is
    // now a `ContextualSearchboxHandler`.
    this.pageHandler().activateMetricsFunnel('AiModeButton');

    chrome.histograms.recordUserAction(
        'ContextualSearch.AiModeButtonClick.NtpRealbox');
    chrome.histograms.recordBoolean(
        'ContextualSearch.AiModeButtonClick.NtpRealbox', true);

    const isSearch = this.selectedMatch?.isSearchType ?? true;
    if (!isSearch) {
      this.setInputText('');
    }
    const queryText = isSearch ? this.$.input.inputElement.value.trim() : '';

    if (queryText) {
      const histogramName =
          'ContextualSearch.UserAction.SubmitQueryV2.NewTabPage';
      // LINT.IfChange(ContextualSearchContextState)
      chrome.histograms.recordEnumerationValue(
          histogramName, /*WithoutContext */ 0,
          /*ContextualSearchContextState.Size + 1*/ 5);
      // LINT.ThenChange(//tools/metrics/histograms/metadata/contextual_search/enums.xml:ContextualSearchContextState)

      const userActionName =
          'ContextualSearch.UserAction.SubmitQueryV2.WithoutContext.NewTabPage';
      chrome.histograms.recordUserAction(userActionName);
    }

    if (!this.composeboxEnabled || queryText) {
      this.pageHandler().notifySessionStarted();
      this.pageHandler().submitQuery(
          queryText, e.detail.button, false, /* altKey */
          e.detail.ctrlKey, e.detail.metaKey, e.detail.shiftKey,
          /* isVoiceSearch */ false);
    } else {
      this.openComposebox_();
    }

    chrome.histograms.recordBoolean(
        'NewTabPage.ComposeEntrypoint.Click.UserTextPresent',
        !this.isInputEmpty());
  }

  protected useCompactLayout_(): boolean {
    return this.searchboxLayoutMode === 'Compact';
  }

  protected openComposebox_(
      uploads: ContextualUpload[] = [], mode: ToolMode = ToolMode.kUnspecified,
      model: ModelMode = ModelMode.kUnspecified, error?: DriveUploadError) {
    if (this.ntpRealboxNextEnabled) {
      const context =
          this.shadowRoot.querySelector<ContextualEntrypointAndMenuElement>(
              '#context');
      assert(context);
      context.closeMenu();
    }

    if (mode !== ToolMode.kUnspecified) {
      recordToolModeSelection(mode, this.composeboxSource, 'ClassicPopup');
    }
    if (model !== ModelMode.kUnspecified) {
      recordModelModeSelection(model, this.composeboxSource, 'ClassicPopup');
    }

    this.fire<ComposeboxState>('open-composebox', {
      text: this.$.input.inputElement.value,
      files: uploads,
      mode: mode,
      model: model,
      error: error,
    });
    this.setInputText('');
  }

  protected onSearchboxInputFilesPasted_(
      e: CustomEvent<{files: FileList}>) {
    this.processFiles_(e.detail.files, ComposeboxContextAddedMethod.COPY_PASTE);
  }

  protected processFiles_(
      files: FileList|null,
      contextAdditionMethod: ComposeboxContextAddedMethod) {
    if (!files || files.length === 0) {
      return;
    }
    recordContextAdditionMethod(contextAdditionMethod, this.composeboxSource);

    if (contextAdditionMethod === ComposeboxContextAddedMethod.CONTEXT_MENU) {
      // In practice, the `files` list will only contain a single file when
      // using the CONTEXT_MENU context addition method in the searchbox.
      for (const file of files) {
        const contextType =
            file.type.includes('image') ? ContextType.IMAGE : ContextType.FILE;
        recordContextualElementClickedMetric(
            this.composeboxSource, 'ClassicPopup', contextType);
      }
    }

    this.openComposebox_(Array.from(files, (file) => ({file})));
  }

  protected onSearchboxInputTextUpdated_(
      e: CustomEvent<{value: string, isComposing: boolean}>) {
    this.onSearchboxInputTextUpdated(e, /*is_composing=*/ false);
  }

  protected onLensSearchClick_() {
    this.dropdownIsVisible = false;
    this.dispatchEvent(new Event('open-lens-search'));
  }

  protected onHadSecondarySideChanged_(e: CustomEvent<{value: boolean}>) {
    this.hadSecondarySide = e.detail.value;
  }

  protected onHasSecondarySideChanged_(e: CustomEvent<{value: boolean}>) {
    this.hasSecondarySide = e.detail.value;
  }

  //============================================================================
  // Helpers
  //============================================================================

  protected inputHasMatches_(): boolean {
    return !!this.result && !!this.result.matches &&
        this.result.matches.length > 0;
  }

  protected computePlaceholderText_(placeholderText: string): string {
    if (placeholderText) {
      return placeholderText;
    }
    return this.i18n('searchBoxHint');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ntp-searchbox': NtpSearchboxElement;
  }
}

customElements.define(NtpSearchboxElement.is, NtpSearchboxElement);
