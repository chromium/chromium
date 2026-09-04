// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './omnibox.js';
import './composebox.js';
import './fre_modal.js';
import '/strings.m.js';
import '//resources/cr_components/composebox/composebox_voice_search.js';
import '//resources/cr_components/most_visited/most_visited.js';
import '//resources/cr_components/search/animated_glow.js';

import {TabUploadOrigin} from '//resources/cr_components/composebox/common.js';
import type {ComposeboxState} from '//resources/cr_components/composebox/common.js';
import type {ComposeboxVoiceSearchElement, VoicePermissionPromptState} from '//resources/cr_components/composebox/composebox_voice_search.js';
import type {MostVisitedElement} from '//resources/cr_components/most_visited/most_visited.js';
import {browserProxyFactory} from '//resources/cr_components/most_visited/most_visited.mojom-webui.js';
import type {MostVisitedInfo} from '//resources/cr_components/most_visited/most_visited.mojom-webui.js';
import type {SearchAnimatedGlowElement} from '//resources/cr_components/search/animated_glow.js';
import {SearchboxBrowserProxy} from '//resources/cr_components/searchbox/searchbox_browser_proxy.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PageCallbackRouter} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {ModelMode, ToolMode} from '//resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {OmniboxEverywhereBrowserProxyImpl} from './browser_proxy.js';
import type {OmniboxEverywhereComposeboxElement} from './composebox.js';
import type {OmniboxEverywhereOmniboxElement} from './omnibox.js';
import type {ComposeboxInitialState} from './omnibox_everywhere.mojom-webui.js';

const PERMISSION_PROMPT_CSS_CLASS = 'permission-prompt-showing';
const VOICE_IDLE_TIMEOUT_MS = 8000;
const VOICE_QUERY_LENGTH_LIMIT = 120;

export interface OmniboxEverywhereAppElement {
  $: {
    content: HTMLElement,
    dialogAnchor: HTMLElement,
    searchbox: OmniboxEverywhereOmniboxElement,
    composebox: OmniboxEverywhereComposeboxElement,
    mostVisited: MostVisitedElement,
    voiceSearchDialog: HTMLDialogElement,
    voiceSearchCardContainer: HTMLElement,
    voiceSearchGlow: SearchAnimatedGlowElement,
    voiceSearch: ComposeboxVoiceSearchElement,
  };
}

export class OmniboxEverywhereAppElement extends CrLitElement {
  static get is() {
    return 'omnibox-everywhere-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  // `#composebox` and `#searchbox` are conditionally rendered in app.html.ts
  // based on `isComposeboxMode_`, so they are mutually exclusive in the DOM.
  get composebox(): OmniboxEverywhereComposeboxElement|null {
    return this.shadowRoot?.querySelector<OmniboxEverywhereComposeboxElement>(
               '#composebox') ??
        null;
  }

  get searchbox(): OmniboxEverywhereOmniboxElement|null {
    return this.shadowRoot?.querySelector<OmniboxEverywhereOmniboxElement>(
               '#searchbox') ??
        null;
  }

  static override get properties() {
    return {
      omniboxPopupDebugEnabled_: {
        type: Boolean,
        reflect: true,
      },
      isComposeboxMode_: {type: Boolean},
      searchboxLayoutMode_: {type: String},
      caretAnimationsEnabled_: {type: Boolean},
      disableComposeboxAnimation_: {type: Boolean},
      usePecApi_: {type: Boolean},
      isOblongShape_: {type: Boolean},
      contextManagementInComposeboxEnabled_: {type: Boolean},
      composeboxState_: {type: Object},
      showVoiceSearchOverlay_: {
        type: Boolean,
        reflect: true,
      },
      hasVoiceSearchError_: {type: Boolean},
      voiceSearchTranscript_: {type: String},
      voiceSearchReceivedSpeech_: {type: Boolean},
      voiceSearchListening_: {type: Boolean},
      voiceIdleTimeoutMs_: {type: Number},
      voiceQueryLengthLimit_: {type: Number},
      callbackRouter_: {type: Object},
      hasMostVisitedTiles_: {type: Boolean},
      mostVisitedEnabled_: {type: Boolean},
      showShortcuts_: {type: Boolean},
      showFreModal_: {type: Boolean},
    };
  }

  protected accessor omniboxPopupDebugEnabled_ =
      loadTimeData.getBoolean('omniboxPopupDebugEnabled');
  protected accessor isComposeboxMode_: boolean = false;
  protected accessor searchboxLayoutMode_: string =
      loadTimeData.getString('searchboxLayoutMode');
  protected accessor caretAnimationsEnabled_: boolean =
      loadTimeData.getBoolean('caretAnimationEnabled');
  protected accessor disableComposeboxAnimation_: boolean =
      loadTimeData.getBoolean('composeboxAnimationDisabled');
  protected accessor usePecApi_: boolean =
      loadTimeData.getBoolean('contextualMenuUsePecApi');
  protected accessor isOblongShape_: boolean =
      loadTimeData.getBoolean('contextButtonShapeIsOblong');
  protected accessor contextManagementInComposeboxEnabled_: boolean =
      loadTimeData.getBoolean('contextManagementInComposeboxEnabled');
  protected accessor composeboxState_: ComposeboxState|null = null;
  protected accessor showVoiceSearchOverlay_: boolean = false;
  protected accessor hasVoiceSearchError_: boolean = false;
  protected accessor voiceSearchTranscript_: string = '';
  protected accessor voiceSearchReceivedSpeech_: boolean = false;
  protected accessor voiceSearchListening_: boolean = false;
  protected accessor voiceIdleTimeoutMs_: number = VOICE_IDLE_TIMEOUT_MS;
  protected accessor voiceQueryLengthLimit_: number = VOICE_QUERY_LENGTH_LIMIT;
  protected accessor callbackRouter_: PageCallbackRouter =
      SearchboxBrowserProxy.getInstance().callbackRouter;
  protected accessor mostVisitedEnabled_: boolean =
      loadTimeData.getBoolean('omniboxEverywhereMostVisitedEnabled');
  protected accessor showShortcuts_: boolean =
      loadTimeData.getBoolean('omniboxEverywhereShowShortcuts');
  protected accessor hasMostVisitedTiles_: boolean = false;
  protected accessor showFreModal_: boolean =
      loadTimeData.getBoolean('initialShowFre');
  private eventTracker_ = new EventTracker();
  private mostVisitedListenerId_: number|null = null;
  private searchboxListenerIds_: number[] = [];
  private omniboxEverywhereListenerIds_: number[] = [];

  override connectedCallback() {
    super.connectedCallback();
    this.eventTracker_.add(
        document.documentElement, 'visibilitychange',
        this.onVisibilitychange_.bind(this));
    this.setupListeners_();
    this.onVisibilitychange_();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
    this.removeListeners_();
  }

  private setupListeners_() {
    const onOpenComposebox = (initialState: ComposeboxInitialState|null) => {
      const state: Partial<ComposeboxState> = {};
      if (initialState) {
        if (initialState.tab) {
          state.files = [{
            ...initialState.tab,
            delayUpload: false,
            origin: TabUploadOrigin.CONTEXT_MENU,
          }];
        }
        if (initialState.fileToken && initialState.fileInfo) {
          state.files = [{
            token: initialState.fileToken,
            fileInfo: initialState.fileInfo,
          }];
        }
        if (initialState.tool !== undefined &&
            initialState.tool !== ToolMode.kUnspecified) {
          state.mode = initialState.tool;
        }
        if (initialState.model !== undefined &&
            initialState.model !== ModelMode.kUnspecified) {
          state.model = initialState.model;
        }
      }
      this.openComposebox(state);
    };

    const onContextMenuClosed = () => {
      this.composebox?.onContextMenuClosed();
      this.searchbox?.onContextMenuClosed();
    };

    this.searchboxListenerIds_.push(
        this.callbackRouter_.setShowFre.addListener((show: boolean) => {
          this.showFreModal_ = show;
        }),
    );

    if (this.mostVisitedEnabled_) {
      this.mostVisitedListenerId_ =
          browserProxyFactory.getInstance()
              .callbackRouter.setMostVisitedInfo.addListener(
                  (info: MostVisitedInfo) => {
                    this.hasMostVisitedTiles_ =
                        info.visible && !!info.tiles && info.tiles.length > 0;
                  });
    }

    const omniboxEverywhereCallbackRouter =
        OmniboxEverywhereBrowserProxyImpl.getInstance().callbackRouter;
    this.omniboxEverywhereListenerIds_.push(
        omniboxEverywhereCallbackRouter.openComposebox.addListener(
            onOpenComposebox),
        omniboxEverywhereCallbackRouter.onContextMenuClosed.addListener(
            onContextMenuClosed),
    );
  }

  private removeListeners_() {
    if (this.mostVisitedListenerId_ !== null) {
      browserProxyFactory.getInstance().callbackRouter.removeListener(
          this.mostVisitedListenerId_);
      this.mostVisitedListenerId_ = null;
    }
    for (const id of this.searchboxListenerIds_) {
      this.callbackRouter_.removeListener(id);
    }
    this.searchboxListenerIds_ = [];
    const omniboxEverywhereCallbackRouter =
        OmniboxEverywhereBrowserProxyImpl.getInstance().callbackRouter;
    for (const id of this.omniboxEverywhereListenerIds_) {
      omniboxEverywhereCallbackRouter.removeListener(id);
    }
    this.omniboxEverywhereListenerIds_ = [];
  }

  /**
   * Opens the Composebox view and hydrates it with any initial contextual
   * state (e.g., pre-uploaded browser files, tab context, active tool modes).
   *
   * This is the centralized transition point invoked by both:
   * 1. Native Views/C++ via Mojo callback (openComposebox listener).
   * 2. WebUI internal action handlers and DOM custom events.
   */
  async openComposebox(state?: Partial<ComposeboxState>) {
    if (this.isComposeboxMode_) {
      if (!this.composebox || !state) {
        return;
      }
      for (const file of state.files ?? []) {
        if ('tabId' in file) {
          this.composebox.addTabContextHandleCallback(file);
        } else if ('token' in file && 'fileInfo' in file) {
          this.composebox.addFileContextFromBrowser(file.token, file.fileInfo);
        }
      }
      if (state.mode !== undefined && state.mode !== ToolMode.kUnspecified) {
        this.composebox.getSearchboxHandler().setActiveToolMode(
            state.mode, false);
      }
      if (state.model !== undefined && state.model !== ModelMode.kUnspecified) {
        this.composebox.getSearchboxHandler().setActiveModelMode(
            state.model, false);
      }
      return;
    }
    // Migrate any typed text from the Omnibox search input into the Composebox
    // state so user queries are preserved when switching to Composebox mode
    // (e.g. when clicking a tool or attaching a tab from the '+' context menu).
    const text = this.searchbox ? this.searchbox.getInputText?.() || '' : '';
    this.composeboxState_ = {
      text,
      files: [],
      mode: ToolMode.kUnspecified,
      model: ModelMode.kUnspecified,
      smartTabSharingActive: false,
      ...state,
    };
    this.isComposeboxMode_ = true;
    await this.updateComplete;
    if (this.composebox) {
      this.composebox.focusInput();
      this.composebox.playGlowAnimation();
    }
    if (this.mostVisitedListenerId_ !== null) {
      browserProxyFactory.getInstance().callbackRouter.removeListener(
          this.mostVisitedListenerId_);
      this.mostVisitedListenerId_ = null;
    }
  }

  protected onFreClose_() {
    const freModal = this.shadowRoot.querySelector('fre-modal');
    if (!freModal) {
      this.showFreModal_ = false;
      SearchboxBrowserProxy.getInstance().handler.dismissFre();
      return;
    }

    freModal.classList.add('dismissing');
    freModal.addEventListener('animationend', () => {
      this.showFreModal_ = false;
      SearchboxBrowserProxy.getInstance().handler.dismissFre();
    }, {once: true});
  }

  protected onFreAcceptHotkey_() {
    this.onFreClose_();
  }

  protected onFreOpenSettings_() {
    SearchboxBrowserProxy.getInstance().handler.openHotkeySettings();
  }

  protected async onOpenComposebox_(e: CustomEvent<ComposeboxState>) {
    this.composeboxState_ = e.detail;
    this.isComposeboxMode_ = true;
    await this.updateComplete;
    const composebox =
        this.shadowRoot?.querySelector<OmniboxEverywhereComposeboxElement>(
            'omnibox-everywhere-composebox');
    if (composebox) {
      composebox.focusInput();
      composebox.playGlowAnimation();
    }
  }

  protected async onCloseComposebox_() {
    this.composeboxState_ = null;
    this.isComposeboxMode_ = false;
    await this.updateComplete;
    const searchbox =
        this.shadowRoot?.querySelector<OmniboxEverywhereOmniboxElement>(
            'omnibox-everywhere-omnibox');
    if (searchbox) {
      searchbox.focusInput();
    }
  }

  protected async onComposeboxSubmit_() {
    this.composeboxState_ = null;
    this.isComposeboxMode_ = false;
    await this.updateComplete;
    const searchbox =
        this.shadowRoot?.querySelector<OmniboxEverywhereOmniboxElement>(
            'omnibox-everywhere-omnibox');
    if (searchbox) {
      searchbox.focusInput();
    }
  }

  private async onVisibilitychange_() {
    if (document.visibilityState !== 'visible') {
      return;
    }

    await this.updateComplete;
    if (this.isComposeboxMode_) {
      const composebox =
          this.shadowRoot?.querySelector<OmniboxEverywhereComposeboxElement>(
              'omnibox-everywhere-composebox');
      if (composebox) {
        composebox.focusInput();
      }
    } else {
      const searchbox =
          this.shadowRoot?.querySelector<OmniboxEverywhereOmniboxElement>(
              'omnibox-everywhere-omnibox');
      if (searchbox) {
        searchbox.focusInput();
      }
    }
  }

  // TODO(b/540973063): Extract common voice search lifecycle handling into
  // SearchboxMixin.
  protected async onOpenVoiceSearch_() {
    this.showVoiceSearchOverlay_ = true;
    this.voiceSearchListening_ = true;
    this.voiceSearchReceivedSpeech_ = false;
    this.voiceSearchTranscript_ = '';
    await this.updateComplete;
    const dialog =
        this.shadowRoot?.querySelector<HTMLDialogElement>('#voiceSearchDialog');
    if (dialog && !dialog.open) {
      dialog.showModal();
    }
    const voiceSearch =
        this.shadowRoot?.querySelector<ComposeboxVoiceSearchElement>(
            '#voiceSearch');
    if (voiceSearch) {
      voiceSearch.start();
    }
  }

  protected onVoiceSearchOverlayClose_() {
    const dialog =
        this.shadowRoot?.querySelector<HTMLDialogElement>('#voiceSearchDialog');
    if (dialog && dialog.open) {
      dialog.close();
    }
    this.showVoiceSearchOverlay_ = false;
    this.hasVoiceSearchError_ = false;
    this.voiceSearchListening_ = false;
  }

  protected onVoicePermissionChanged_(
      e: CustomEvent<VoicePermissionPromptState>) {
    if (e.detail.isOpened) {
      this.voiceSearchListening_ = false;
    } else {
      this.voiceSearchListening_ =
          this.showVoiceSearchOverlay_ && !this.hasVoiceSearchError_;
    }
    const audioAnimation =
        this.shadowRoot?.querySelector<SearchAnimatedGlowElement>(
            '#voiceSearchGlow');
    if (audioAnimation) {
      if (e.detail.isOpened) {
        audioAnimation.classList.add(PERMISSION_PROMPT_CSS_CLASS);
      } else {
        audioAnimation.classList.remove(PERMISSION_PROMPT_CSS_CLASS);
      }
    }
    const voiceSearchElement =
        this.shadowRoot?.querySelector<ComposeboxVoiceSearchElement>(
            '#voiceSearch');
    if (voiceSearchElement) {
      if (e.detail.isOpened) {
        voiceSearchElement.classList.add(PERMISSION_PROMPT_CSS_CLASS);
      } else {
        voiceSearchElement.classList.remove(PERMISSION_PROMPT_CSS_CLASS);
      }
    }
  }

  protected onVoiceSearchCancel_() {
    this.onVoiceSearchOverlayClose_();
  }

  protected onVoiceSearchError_() {
    if (!this.showVoiceSearchOverlay_) {
      return;
    }
    this.hasVoiceSearchError_ = true;
  }

  protected onVoiceSearchRestart_() {
    this.hasVoiceSearchError_ = false;
    this.voiceSearchListening_ = true;
    this.voiceSearchReceivedSpeech_ = false;
    this.voiceSearchTranscript_ = '';
  }

  protected onVoiceSearchTranscriptUpdate_(e: CustomEvent<string>) {
    this.voiceSearchTranscript_ = e.detail;
  }

  protected onVoiceSearchSpeechReceived_() {
    this.voiceSearchReceivedSpeech_ = true;
  }

  protected onVoiceSearchDialogClick_(e: MouseEvent) {
    const dialog = e.currentTarget as HTMLDialogElement;
    if (e.target === dialog) {
      this.onVoiceSearchOverlayClose_();
    }
  }

  private handleVoiceSearchResult_(query: string, submit: boolean) {
    this.onVoiceSearchOverlayClose_();
    const trimmedQuery = query?.trim();
    if (!trimmedQuery) {
      return;
    }

    if (this.isComposeboxMode_) {
      const composebox = this.$.composebox;
      if (composebox) {
        composebox.setInputText(trimmedQuery);
        if (submit) {
          composebox.getSearchboxHandler().submitQuery(
              trimmedQuery, /*mouse_button=*/ 0, /*alt_key=*/ false,
              /*ctrl_key=*/ false, /*meta_key=*/ false, /*shift_key=*/ false,
              /*is_voice_search=*/ true);
          composebox.submitCleanup();
        } else {
          composebox.focusInput();
          composebox.queryAutocomplete(/*clearMatches=*/ false);
        }
      }
    } else {
      const searchbox = this.$.searchbox;
      if (searchbox) {
        searchbox.setInputText(trimmedQuery);
        if (submit) {
          searchbox.pageHandler().submitQuery(
              trimmedQuery, /*mouse_button=*/ 0, /*alt_key=*/ false,
              /*ctrl_key=*/ false, /*meta_key=*/ false, /*shift_key=*/ false,
              /*is_voice_search=*/ true);
          searchbox.clearAutocompleteMatches();
          searchbox.setInputText('');
        } else {
          searchbox.focusInput();
          searchbox.queryAutocomplete(trimmedQuery, false, false);
        }
      }
    }
  }

  protected onVoiceSearchFinalResult_(e: CustomEvent<string>) {
    this.handleVoiceSearchResult_(e.detail, /*submit=*/ true);
  }

  protected onVoiceSearchRecordingStopped_(e: CustomEvent<string>) {
    this.handleVoiceSearchResult_(e.detail, /*submit=*/ false);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'omnibox-everywhere-app': OmniboxEverywhereAppElement;
  }
}

customElements.define(
    OmniboxEverywhereAppElement.is, OmniboxEverywhereAppElement);
