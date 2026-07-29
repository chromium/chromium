// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_components/searchbox/searchbox_dropdown.js';
import '//resources/cr_components/searchbox/searchbox_input.js';
import '//resources/cr_components/searchbox/searchbox_compose_button.js';
import '//resources/cr_components/search/animated_glow.js';
import '//resources/cr_components/composebox/composebox_file_inputs.js';
import '//resources/cr_components/composebox/contextual_entrypoint_and_menu.js';

import {ContextType, GlifAnimationState, recordContextAdditionMethod, recordContextualElementClickedMetric, TabSuggestionsState} from '//resources/cr_components/composebox/common.js';
import type {ComposeboxState, ContextualUpload, DriveUpload, TabUpload, TabUploadOrigin} from '//resources/cr_components/composebox/common.js';
import type {ComposeboxFileInputsElement} from '//resources/cr_components/composebox/composebox_file_inputs.js';
import {ComposeboxContextAddedMethod, GlowAnimationState} from '//resources/cr_components/search/constants.js';
import {SearchboxBrowserProxy} from '//resources/cr_components/searchbox/searchbox_browser_proxy.js';
import type {SearchboxDropdownElement} from '//resources/cr_components/searchbox/searchbox_dropdown.js';
import type {SearchboxInputElement} from '//resources/cr_components/searchbox/searchbox_input.js';
import type {SearchboxMixinInterface} from '//resources/cr_components/searchbox/searchbox_mixin.js';
import {SearchboxMixin} from '//resources/cr_components/searchbox/searchbox_mixin.js';
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
//       I18nMixinLit may eventually be moved to SearchboxMixin.
const OmniboxEverywhereOmniboxElementBase =
    SearchboxMixin(I18nMixinLit(WebUiListenerMixinLit(CrLitElement)));

export class OmniboxEverywhereOmniboxElement extends
    OmniboxEverywhereOmniboxElementBase implements SearchboxMixinInterface {
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
      useWebkitSearchIcons_: {
        type: Boolean,
        reflect: true,
      },
      animationState_: {type: String},
      composeButtonEnabled: {type: Boolean, reflect: true},
      ntpRealboxNextEnabled: {type: Boolean, reflect: true},
      profileAvatarUrl_: {type: String},
      contextMenuGlifAnimationState: {
        type: String,
        reflect: true,
      },
      inputState_: {type: Object},
      tabSuggestions_: {type: Array},
      searchboxLayoutMode: {type: String},
      tabSuggestionsState_: {type: Number},
      contextManagementInComposeboxEnabled: {type: Boolean},
    };
  }

  accessor placeholderText: string = '';
  accessor searchboxChromeRefreshTheming: boolean =
      loadTimeData.getBoolean('searchboxCr23Theming');
  accessor searchboxSteadyStateShadow: boolean =
      loadTimeData.getBoolean('searchboxCr23SteadyStateShadow');
  accessor contextManagementInComposeboxEnabled: boolean = false;
  protected accessor searchboxIcon_: string =
      '//resources/cr_components/searchbox/icons/google_g.svg';
  protected accessor searchboxVoiceSearchEnabled_: boolean =
      loadTimeData.getBoolean('searchboxVoiceSearch');
  protected accessor searchboxLensSearchEnabled_: boolean =
      loadTimeData.getBoolean('searchboxLensSearch');
  protected accessor useWebkitSearchIcons_: boolean = true;
  protected accessor animationState_: GlowAnimationState =
      GlowAnimationState.NONE;
  protected accessor composeButtonEnabled: boolean =
      loadTimeData.getBoolean('searchboxShowComposeEntrypoint');
  protected accessor ntpRealboxNextEnabled: boolean =
      loadTimeData.getBoolean('ntpRealboxNextEnabled');
  protected accessor profileAvatarUrl_: string =
      loadTimeData.getString('profileAvatarUrl');
  accessor contextMenuGlifAnimationState: GlifAnimationState =
      GlifAnimationState.STARTED;
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

  constructor() {
    super();
    const browserProxy = SearchboxBrowserProxy.getInstance();
    this.pageHandler_ = browserProxy.handler;
    this.callbackRouter_ = browserProxy.callbackRouter;
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
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('searchboxChromeRefreshTheming')) {
      this.useWebkitSearchIcons_ = this.searchboxChromeRefreshTheming;
    }
  }

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);
    this.initialInputScrollHeight = this.$.input.scrollHeight;
  }

  focusInput() {
    this.$.input.focus();
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

  //========================================================================
  // Event handlers
  //========================================================================

  protected onInputFocusin_() {
    this.pageHandler_.onFocusChanged(true);
  }

  protected computePlaceholderText_(): string {
    if (this.placeholderText) {
      return this.placeholderText;
    }
    if (this.ntpRealboxNextEnabled) {
      return this.i18n('searchBoxHintAskOrType');
    }
    return this.i18n('searchBoxHint');
  }

  protected onSearchboxInputTextUpdated_(
      e: CustomEvent<{value: string, isComposing: boolean}>) {
    this.onSearchboxInputTextUpdated(e);
  }

  protected onVoiceSearchClick_() {
    this.dispatchEvent(new Event('open-voice-search'));
  }

  protected onLensSearchClick_() {
    this.dropdownIsVisible = false;
    this.dispatchEvent(new Event('open-lens-search'));
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
  }

  protected async openComposeboxWithMode_(mode?: ToolMode, model?: ModelMode) {
    this.animationState_ = GlowAnimationState.NONE;
    await this.updateComplete;
    this.animationState_ = GlowAnimationState.LISTENING;
    setTimeout(() => {
      this.openComposebox_([], mode, model);
    }, 300);
  }

  protected onComposeClick_() {
    this.openComposeboxWithMode_();
  }

  protected onContextMenuEntrypointClick_() {
    this.pageHandler().activateMetricsFunnel('PlusButton');
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
