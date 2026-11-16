// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './omnibox.js';
import './composebox.js';
import './fre_modal.js';
import './fre_chin.js';
import '/strings.m.js';
import '//resources/cr_components/composebox/composebox_voice_search.js';
import '//resources/cr_components/most_visited/most_visited.js';
import '//resources/cr_components/search/animated_glow.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/icons.html.js';

import {ColorChangeUpdater} from '//resources/cr_components/color_change_listener/colors_css_updater.js';
import type {ComposeboxState} from '//resources/cr_components/composebox/common.js';
import type {ComposeboxVoiceSearchElement, VoicePermissionPromptState} from '//resources/cr_components/composebox/composebox_voice_search.js';
import {browserProxyFactory} from '//resources/cr_components/most_visited/most_visited.mojom-webui.js';
import type {MostVisitedInfo} from '//resources/cr_components/most_visited/most_visited.mojom-webui.js';
import type {SearchAnimatedGlowElement} from '//resources/cr_components/search/animated_glow.js';
import {SearchboxBrowserProxy} from '//resources/cr_components/searchbox/searchbox_browser_proxy.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {getDeepActiveElement} from '//resources/js/util.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {FreStage, TabAttachmentSource} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {FreState, PageCallbackRouter} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {ModelMode, ToolMode} from '//resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {OmniboxEverywhereBrowserProxyImpl} from './browser_proxy.js';
import type {OmniboxEverywhereComposeboxElement} from './composebox.js';
import {FreChinMode} from './fre_chin.js';
import type {ShowHotkeyDropdownDetail} from './fre_chin.js';
import type {OmniboxEverywhereOmniboxElement} from './omnibox.js';
import type {ComposeboxInitialState} from './omnibox_everywhere.mojom-webui.js';

const PERMISSION_PROMPT_CSS_CLASS = 'permission-prompt-showing';
const VOICE_IDLE_TIMEOUT_MS = 8000;
const VOICE_QUERY_LENGTH_LIMIT = 120;
// Duration after a window activation focus event during which a click is
// still treated as part of the activation gesture. The activating click
// reaches the renderer shortly after the focus event that precedes it, so
// this only needs to absorb that delay while staying well below the
// timescale of a deliberate follow-up interaction.
const ACTIVATION_CLICK_WINDOW_MS = 300;

export interface OmniboxEverywhereAppElement {
  $: {
    content: HTMLElement,
    dialogAnchor: HTMLElement,
  };
}

const OmniboxEverywhereAppElementBase = I18nMixinLit(CrLitElement);

export class OmniboxEverywhereAppElement extends
    OmniboxEverywhereAppElementBase {
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
      isComposeboxMode_: {type: Boolean},
      searchboxLayoutMode_: {type: String},
      usePecApi_: {type: Boolean},
      isOblongShape_: {type: Boolean},
      contextManagementInComposeboxEnabled_: {type: Boolean},
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
      smallLoomnibox_: {
        type: Boolean,
        reflect: true,
        attribute: 'small-loomnibox',
      },
      dynamicSizing_: {
        type: Boolean,
        reflect: true,
        attribute: 'dynamic-sizing',
      },
      dynamicSizingWidth_: {
        type: Number,
        reflect: true,
        attribute: 'dynamic-sizing-width',
      },
      smallFont_: {
        type: Boolean,
        reflect: true,
        attribute: 'small-font',
      },
      hideTitle_: {type: Boolean},
      freStage_: {type: Number},
      hotkeyTokens_: {type: Array},
      isHotkeyDropdownOpen_: {type: Boolean},
      isActive_: {
        type: Boolean,
        reflect: true,
        attribute: 'is-active',
      },
      isExpanded_: {type: Boolean},
      isPendingContextUpload_: {type: Boolean},
    };
  }

  protected accessor isHotkeyDropdownOpen_: boolean = false;
  protected accessor isActive_: boolean = true;
  private accessor isExpanded_: boolean = true;
  protected accessor isComposeboxMode_: boolean = false;
  protected accessor isPendingContextUpload_: boolean = false;
  protected accessor searchboxLayoutMode_: string =
      loadTimeData.getString('searchboxLayoutMode');
  protected accessor usePecApi_: boolean =
      loadTimeData.getBoolean('contextualMenuUsePecApi');
  protected accessor isOblongShape_: boolean =
      loadTimeData.getBoolean('contextButtonShapeIsOblong');
  protected accessor contextManagementInComposeboxEnabled_: boolean =
      loadTimeData.getBoolean('contextManagementInComposeboxEnabled');
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
  protected accessor smallLoomnibox_: boolean =
      loadTimeData.getBoolean('smallLoomnibox');
  protected accessor dynamicSizing_: boolean =
      loadTimeData.getBoolean('dynamicSizing');
  protected accessor dynamicSizingWidth_: number =
      loadTimeData.getInteger('dynamicSizingWidth');
  protected accessor smallFont_: boolean = loadTimeData.getBoolean('smallFont');
  protected accessor hideTitle_: boolean =
      loadTimeData.getBoolean('omniboxEverywhereMostVisitedHideTitle');
  private accessor hasMostVisitedTiles_: boolean = false;
  private accessor freStage_: FreStage =
      (loadTimeData.valueExists('initialFreStage') ?
           loadTimeData.getInteger('initialFreStage') :
           FreStage.kNone) as FreStage;
  protected accessor hotkeyTokens_: string[] =
      loadTimeData.valueExists('initialHotkeyTokens') ?
      loadTimeData.getValue('initialHotkeyTokens') :
      [];

  private isPersistentMode_: boolean =
      loadTimeData.getBoolean('isPersistentMode');
  // Tracks whether the current pending context upload (screenshot capture or
  // native file chooser) was initiated while the user was in the Searchbox
  // (`!isComposeboxMode_`) rather than already inside the Composebox. If the
  // user cancels the picker, this flag tells `onScreenshotCaptureCancelled` to
  // return the view back to the Searchbox instead of leaving an empty
  // Composebox open.
  private contextUploadOriginWasSearchbox_: boolean = false;
  private eventTracker_ = new EventTracker();
  private mostVisitedListenerId_: number|null = null;
  private searchboxListenerIds_: number[] = [];
  private omniboxEverywhereListenerIds_: number[] = [];
  private wasJustActivated_ = false;
  private activationTimeoutId_: number|null = null;
  private impressionRecordedForSession_ = false;

  constructor() {
    super();
    ColorChangeUpdater.forDocument().start();
  }

  override connectedCallback() {
    super.connectedCallback();
    ColorChangeUpdater.forDocument().refreshColorsCss();
    this.isActive_ = !this.isPersistentMode_ || document.hasFocus();
    this.eventTracker_.add(window, 'focus', this.onWindowFocus_.bind(this));
    this.eventTracker_.add(window, 'blur', this.onWindowBlur_.bind(this));
    this.eventTracker_.add(window, 'click', this.onAppClick_.bind(this));
    this.eventTracker_.add(this, 'text-input-focused', () => {
      this.isExpanded_ = true;
    });
    this.eventTracker_.add(
        document, 'visibilitychange', this.onVisibilitychange_.bind(this));
    this.eventTracker_.add(
        window, 'keydown', (e: KeyboardEvent) => this.onKeyDown_(e));
    this.setupListeners_();
    this.onVisibilitychange_();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.clearActivationTimeout_();
    this.eventTracker_.removeAll();
    this.removeListeners_();
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    if (this.dynamicSizing_) {
      // In persistent mode, rest in the small 480px state (4 MVT tiles) until
      // the text input is focused; then stay expanded (5 tiles) until the
      // browser demotes the widget (`onDemoted_`). Transient window blurs
      // (e.g. native menus) intentionally do not shrink it.
      this.smallLoomnibox_ = this.isResting_();
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    this.maybeRecordFreImpression_();
  }

  private setupListeners_() {
    const onOpenComposebox = (initialState: ComposeboxInitialState|null) => {
      const state: Partial<ComposeboxState> = {};
      let isPendingContextUpload = false;
      if (initialState) {
        if (initialState.tab) {
          state.files = [{
            ...initialState.tab,
            delayUpload: false,
            origin: TabAttachmentSource.kContextMenu,
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
        if (initialState.isPendingContextUpload) {
          isPendingContextUpload = true;
        }
      }
      this.openComposebox(state, isPendingContextUpload);
    };

    const onContextMenuClosed = () => {
      this.composebox?.onContextMenuClosed();
      this.searchbox?.onContextMenuClosed();
      // The composebox restores input focus after its context menu closes,
      // except at the 480px resting size, where focusing would expand it.
      if (this.isComposeboxMode_ && !this.isResting_()) {
        this.focusActiveInput_();
      }
    };

    // Invoked when a pending screenshot capture or native file chooser is
    // cancelled. If the upload flow started from the Searchbox (which
    // pre-transitioned into the collapsed Composebox pill so the widget could
    // shrink before presenting the native UI), restore the Searchbox view and
    // focus its input; otherwise keep the existing Composebox open and restore
    // its suggestions.
    // TODO(b/581982262): Rename `onScreenshotCaptureCancelled` (and its mojom
    // method) to `onContextUploadCancelled` now that it is shared with the
    // native file chooser.
    const onScreenshotCaptureCancelled = async () => {
      this.isPendingContextUpload_ = false;
      if (this.contextUploadOriginWasSearchbox_) {
        this.contextUploadOriginWasSearchbox_ = false;
        // TODO(b/585172152): Preserve any query text typed in the searchbox
        // when reverting a cancelled screenshot or file upload back to the
        // searchbox.
        this.setIsComposebox_(false);
        await this.updateComplete;
        this.searchbox?.focusInput?.();
        return;
      }
      this.composebox?.onScreenshotCaptureCancelled();
    };

    this.searchboxListenerIds_.push(
        this.callbackRouter_.setFreState.addListener((state: FreState) => {
          if (this.freStage_ !== state.stage) {
            this.impressionRecordedForSession_ = false;
          }
          this.freStage_ = state.stage;
          this.hotkeyTokens_ = state.currentHotkeyTokens;
          const freElement =
              this.shadowRoot?.querySelector('fre-modal, fre-chin');
          if (freElement) {
            freElement.classList.remove('dismissing');
          }
        }),
        this.callbackRouter_.updateAimPopupEligibility.addListener(
            (aiModePrefEnabled: boolean) => {
              if (!aiModePrefEnabled && this.isComposeboxMode_) {
                this.onCloseComposebox_();
              }
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
        omniboxEverywhereCallbackRouter.onOverflowMenuClosed.addListener(
            () => this.searchbox?.onOverflowMenuClosed()),
        omniboxEverywhereCallbackRouter.onScreenshotCaptureCancelled
            .addListener(onScreenshotCaptureCancelled),
        omniboxEverywhereCallbackRouter.focusInput.addListener(() => {
          // Expand even if the input can't take focus (e.g. under the FRE
          // intro modal).
          this.isExpanded_ = true;
          this.focusActiveInput_();
        }),
        omniboxEverywhereCallbackRouter.onDemoted.addListener(() => {
          this.onDemoted_();
        }),
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
  async openComposebox(
      state?: Partial<ComposeboxState>,
      isPendingContextUpload: boolean = false) {
    if (this.isComposeboxMode_) {
      this.isPendingContextUpload_ = isPendingContextUpload;
      this.contextUploadOriginWasSearchbox_ = false;
      if (this.composebox && state) {
        for (const file of state.files ?? []) {
          if ('tabId' in file) {
            this.composebox.addTabContextHandleCallback(file);
          } else if ('token' in file && 'fileInfo' in file) {
            this.composebox.addFileContextFromBrowser(
                file.token, file.fileInfo);
          }
        }
        if (state.mode !== undefined && state.mode !== ToolMode.kUnspecified) {
          this.composebox.getSearchboxHandler().setActiveToolMode(
              state.mode, false);
        }
        if (state.model !== undefined &&
            state.model !== ModelMode.kUnspecified) {
          this.composebox.getSearchboxHandler().setActiveModelMode(
              state.model, false);
        }
      }
      if (this.composebox) {
        this.composebox.isPendingContextUpload = isPendingContextUpload;
        // Focus the input after a + menu action (e.g. picking a model), as
        // entering composebox mode does below.
        if (!isPendingContextUpload && state) {
          this.composebox.focusInput();
        }
      }
      return;
    }
    // Migrate any typed text from the Omnibox search input into the Composebox
    // state so user queries are preserved when switching to Composebox mode
    // (e.g. when clicking a tool or attaching a tab from the '+' context menu).
    const text = this.searchbox ? this.searchbox.getInputText?.() || '' : '';
    // Calls setActiveToolMode(initialMode) directly without passing mode down
    // through composebox.state. This prevents
    // ComposeboxEmbedderMixin.updateState() from mistakenly calling
    // handleToolClick() and toggling off the active tool mode.
    const {mode: initialMode = ToolMode.kUnspecified, ...stateForComposebox} =
        state ?? {};

    const composeboxState: ComposeboxState = {
      text,
      files: [],
      mode: ToolMode.kUnspecified,
      model: ModelMode.kUnspecified,
      smartTabSharingActive: false,
      ...stateForComposebox,
    };
    if (initialMode !== ToolMode.kUnspecified) {
      SearchboxBrowserProxy.getInstance().handler.setActiveToolMode(
          initialMode, false);
    }
    // Set isPendingContextUpload_ for this transition into Composebox mode so
    // that the newly mounted composebox element receives the property binding
    // and suppresses suggestions / glow animation until capture completes.
    this.isPendingContextUpload_ = isPendingContextUpload;
    this.contextUploadOriginWasSearchbox_ = isPendingContextUpload;
    // Clear any active autocomplete suggestion matches on the omnibox searchbox
    // so stale dropdown results do not linger or flash when switching modes or
    // if the user subsequently exits Composebox back to Omnibox.
    this.searchbox?.clearAutocompleteMatches?.();
    this.setIsComposebox_(true);
    await this.updateComplete;
    if (this.composebox) {
      this.composebox.state = composeboxState;
      this.composebox.focusInput();
      if (!isPendingContextUpload) {
        this.composebox.playGlowAnimation();
        this.composebox.queryAutocomplete(/*clearMatches=*/ true);
      }
    }
  }

  protected isFreIntroModal_(): boolean {
    return this.freStage_ === FreStage.kIntroModal && !this.isComposeboxMode_;
  }

  protected isFreChin_(): boolean {
    return (this.freStage_ === FreStage.kShortcutSetupChin ||
            this.freStage_ === FreStage.kShortcutReminderChin) &&
        !this.isComposeboxMode_;
  }

  private maybeRecordFreImpression_() {
    if (this.impressionRecordedForSession_ || !this.isActive_ ||
        document.visibilityState !== 'visible') {
      return;
    }
    let selector: string|null = null;
    if (this.isFreIntroModal_()) {
      selector = 'fre-modal';
    } else if (this.isFreShortcutSetupChin_()) {
      selector = '#freShortcutSetupChin';
    } else if (this.isFreShortcutReminderChin_()) {
      selector = '#freShortcutReminderChin';
    }
    if (!selector || !this.shadowRoot?.querySelector(selector)) {
      return;
    }
    this.impressionRecordedForSession_ = true;
    SearchboxBrowserProxy.getInstance().handler.recordFreImpression(
        this.freStage_);
  }

  protected isMostVisitedHidden_(): boolean {
    return !this.hasMostVisitedTiles_ || this.isFreIntroModal_() ||
        this.isFreChin_();
  }

  protected isFreShortcutSetupChin_(): boolean {
    return this.freStage_ === FreStage.kShortcutSetupChin &&
        !this.isComposeboxMode_;
  }

  protected isFreShortcutReminderChin_(): boolean {
    return this.freStage_ === FreStage.kShortcutReminderChin &&
        !this.isComposeboxMode_;
  }

  protected getFreChinMode_(): FreChinMode {
    return this.freStage_ === FreStage.kShortcutSetupChin ?
        FreChinMode.SHORTCUT_SETUP :
        FreChinMode.SHORTCUT_REMINDER;
  }

  protected getMaxMostVisitedTiles_(): number {
    return this.smallLoomnibox_ ? 4 : 5;
  }

  protected onMostVisitedCloseClick_() {
    this.hasMostVisitedTiles_ = false;
    OmniboxEverywhereBrowserProxyImpl.getInstance()
        .handler.onDismissMostVisited();
  }

  private setIsComposebox_(isComposebox: boolean) {
    if (this.isComposeboxMode_ === isComposebox) {
      return;
    }
    this.isComposeboxMode_ = isComposebox;
    OmniboxEverywhereBrowserProxyImpl.getInstance().handler.setIsComposebox(
        isComposebox);
  }

  protected onFreClose_() {
    const stageToDismiss = this.freStage_;
    const freElement = this.shadowRoot?.querySelector('fre-modal, fre-chin');
    if (!freElement) {
      SearchboxBrowserProxy.getInstance().handler.dismissFre(stageToDismiss);
      return;
    }
    if (freElement.classList.contains('dismissing')) {
      return;
    }

    freElement.classList.add('dismissing');

    let dismissed = false;
    const finishDismissal = () => {
      if (dismissed) {
        return;
      }
      dismissed = true;
      SearchboxBrowserProxy.getInstance().handler.dismissFre(stageToDismiss);
    };

    // Use a single named function so we can remove it accurately.
    const onAnimationDone = (e: Event) => {
      const animEvent = e as AnimationEvent;
      if (animEvent.target === freElement &&
          animEvent.animationName === 'fadeOutFre') {
        // Only remove the listeners once our specific animation is handled.
        freElement.removeEventListener('animationend', onAnimationDone);
        freElement.removeEventListener('animationcancel', onAnimationDone);
        finishDismissal();
      }
    };

    freElement.addEventListener('animationend', onAnimationDone);
    freElement.addEventListener('animationcancel', onAnimationDone);
  }

  protected async onFreShowHotkeyDropdown_(
      e: CustomEvent<ShowHotkeyDropdownDetail>) {
    if (this.isHotkeyDropdownOpen_) {
      return;
    }
    this.isHotkeyDropdownOpen_ = true;
    try {
      await SearchboxBrowserProxy.getInstance().handler.showHotkeyDropdown(
          e.detail);
    } finally {
      this.isHotkeyDropdownOpen_ = false;
      // Defer to a macrotask so in-flight window focus/blur events settle
      // before sampling document focus.
      setTimeout(() => {
        if (this.isConnected && !this.isHotkeyDropdownOpen_) {
          this.isActive_ = !this.isPersistentMode_ || document.hasFocus();
        }
      }, 0);
    }
  }

  protected onFreOpenSettings_() {
    SearchboxBrowserProxy.getInstance().handler.openHotkeySettings();
  }
  protected async onOpenComposebox_(e: CustomEvent<ComposeboxState>) {
    this.contextUploadOriginWasSearchbox_ = false;
    this.setIsComposebox_(true);
    await this.updateComplete;
    const composebox =
        this.shadowRoot?.querySelector<OmniboxEverywhereComposeboxElement>(
            'omnibox-everywhere-composebox');
    if (composebox) {
      composebox.state = e.detail;
      composebox.focusInput();
      composebox.playGlowAnimation();
    }
  }

  protected async onCloseComposebox_() {
    this.isPendingContextUpload_ = false;
    this.contextUploadOriginWasSearchbox_ = false;
    this.setIsComposebox_(false);
    await this.updateComplete;
    if (!this.isResting_()) {
      this.focusActiveInput_();
    }
  }

  private onKeyDown_(e: KeyboardEvent) {
    if (e.key !== 'Escape' || e.defaultPrevented) {
      return;
    }
    if (this.showVoiceSearchOverlay_) {
      return;
    }

    if (this.isComposeboxMode_) {
      const composebox =
          this.shadowRoot?.querySelector<OmniboxEverywhereComposeboxElement>(
              'omnibox-everywhere-composebox');
      if (composebox) {
        composebox.handleEscapeKeyLogic();
        e.preventDefault();
        return;
      }
    } else {
      const searchbox =
          this.shadowRoot?.querySelector<OmniboxEverywhereOmniboxElement>(
              'omnibox-everywhere-omnibox');
      if (searchbox && searchbox.getInputText()) {
        searchbox.setInputText('');
        searchbox.clearAutocompleteMatches();
        searchbox.focusInput();
        e.preventDefault();
        return;
      }
    }

    SearchboxBrowserProxy.getInstance().handler.onEscapePressed();
    e.preventDefault();
  }

  protected async onComposeboxSubmit_() {
    this.isComposeboxMode_ = false;
    this.isPendingContextUpload_ = false;
    this.contextUploadOriginWasSearchbox_ = false;
    await this.updateComplete;
    if (!this.isResting_()) {
      this.focusActiveInput_();
    }
  }

  // Whether the window is in its 480px resting state (persistent dynamic sizing
  // mode, not yet expanded by the text input). Focusing the input expands it,
  // so automatic refocusing is skipped while resting.
  private isResting_(): boolean {
    return this.dynamicSizing_ && this.isPersistentMode_ && !this.isExpanded_;
  }

  private shouldAutoFocusInput_(): boolean {
    if (this.isFreIntroModal_() || this.showVoiceSearchOverlay_) {
      return false;
    }
    const openDialog = this.shadowRoot?.querySelector('dialog[open]');
    if (openDialog) {
      return false;
    }
    return true;
  }

  private focusActiveInput_() {
    if (!this.shouldAutoFocusInput_()) {
      return;
    }
    if (this.isComposeboxMode_) {
      this.composebox?.focusInput();
    } else {
      this.searchbox?.focusInput();
    }
    this.isExpanded_ = true;
  }

  private onDemoted_() {
    this.isExpanded_ = false;
    // Clear active element focus on demotion so Blink's `FocusController`
    // does not auto-restore `<input>` focus when `web_contents()->Focus()`
    // runs on `WM_ACTIVATE(WA_CLICKACTIVE)` before `WM_LBUTTONDOWN`.
    const activeEl = getDeepActiveElement();
    if (activeEl instanceof HTMLElement && activeEl !== document.body) {
      activeEl.blur();
    }
    this.clearActivationTimeout_();
  }

  private onWindowFocus_() {
    this.isActive_ = true;
    this.maybeRecordFreImpression_();
    // In persistent dynamic sizing mode, only the text input expands the
    // window. Activation (e.g. a click on a button, a drag, or closing a menu)
    // only restores input focus if the window is already expanded.
    if (this.isResting_()) {
      return;
    }
    this.focusActiveInput_();
    this.wasJustActivated_ = true;
    if (this.activationTimeoutId_ !== null) {
      clearTimeout(this.activationTimeoutId_);
    }
    this.activationTimeoutId_ = setTimeout(() => {
      this.wasJustActivated_ = false;
      this.activationTimeoutId_ = null;
    }, ACTIVATION_CLICK_WINDOW_MS);
  }

  private onWindowBlur_() {
    // Ignore transient `window` blur events while a native modal UI (the hotkey
    // dropdown, screenshot picker, or native file chooser sheet) temporarily
    // takes OS window focus away from the Omnibox Everywhere WebContents.
    // Otherwise, in persistent mode, `this.isActive_ = false` would shrink the
    // widget width (`smallLoomnibox_ = true`) while the native sheet/picker is
    // anchored to it.
    //
    // Once `<omnibox-everywhere-composebox>` is mounted, read its live
    // `composebox.isPendingContextUpload` property rather than `app.ts`'s
    // initial `this.isPendingContextUpload_` field: when a file upload
    // completes (`addFileContextFromBrowser`) or the user types in the input
    // (`onInputInput`), `composebox.ts` clears its own `isPendingContextUpload`
    // flag directly without notifying `app.ts`, so consulting `this.composebox`
    // ensures subsequent real window blurs deactivate the app normally.
    if (this.isHotkeyDropdownOpen_ ||
        (this.composebox ? this.composebox.isPendingContextUpload :
                           this.isPendingContextUpload_)) {
      return;
    }
    if (this.isPersistentMode_) {
      this.isActive_ = false;
    }
    this.clearActivationTimeout_();
  }

  private onAppClick_() {
    if (!this.wasJustActivated_) {
      return;
    }
    this.clearActivationTimeout_();
    // Only restore focus if the click left nothing focused. If it landed on
    // a focusable control, that control should keep focus.
    if (getDeepActiveElement() === document.body) {
      this.focusActiveInput_();
    }
  }

  private clearActivationTimeout_() {
    this.wasJustActivated_ = false;
    if (this.activationTimeoutId_ !== null) {
      clearTimeout(this.activationTimeoutId_);
      this.activationTimeoutId_ = null;
    }
  }

  private async onVisibilitychange_() {
    if (document.visibilityState !== 'visible') {
      this.impressionRecordedForSession_ = false;
      return;
    }

    await this.updateComplete;
    this.maybeRecordFreImpression_();
    if (this.isResting_()) {
      return;
    }
    this.focusActiveInput_();
  }

  // TODO(b/540973063): Extract common voice search lifecycle handling into
  // SearchboxMixin.
  protected async onOpenVoiceSearch_() {
    this.isExpanded_ = true;
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
    e.preventDefault();
    if (e.detail.isOpened) {
      this.voiceSearchListening_ = false;
    } else {
      this.voiceSearchListening_ =
          this.showVoiceSearchOverlay_ && !this.hasVoiceSearchError_;
    }

    this.classList.toggle('has-permission-prompt', e.detail.isOpened);
    if (e.detail.isOpened) {
      this.style.setProperty(
          '--voice_search_minimum_height', `${e.detail.height}px`);
      this.style.setProperty(
          '--voice_search_minimum_width', `${e.detail.width}px`);
    } else {
      this.style.removeProperty('--voice_search_minimum_height');
      this.style.removeProperty('--voice_search_minimum_width');
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
      const composebox =
          this.shadowRoot.querySelector<OmniboxEverywhereComposeboxElement>(
              '#composebox');
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
      const searchbox =
          this.shadowRoot.querySelector<OmniboxEverywhereOmniboxElement>(
              '#searchbox');
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
