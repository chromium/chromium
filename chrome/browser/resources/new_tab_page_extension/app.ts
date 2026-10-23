// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Must be imported first so that `loadTimeData` is populated before any
// element below reads from it.
import '/strings.m.js';
import '/ntp/action_chips/action_chips.js';
import '/ntp/lens_upload_dialog.js';
import '/ntp/logo.js';
import '/ntp/ntp_composebox.js';
import '/ntp/ntp_searchbox.js';
import 'chrome://resources/cr_components/composebox/composebox_voice_search.js';
import 'chrome://resources/cr_components/search/animated_glow.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';

import type {ActionChipClickDetail} from '/ntp/action_chips/action_chips.js';
import {ActionChipsRetrievalState} from '/ntp/action_chips/action_chips.js';
import {recordBoolean} from '/ntp/metrics_utils.js';
import type {PageCallbackRouter, PageHandlerRemote, Theme} from '/ntp/new_tab_page.mojom-webui.js';
import {NewTabPageProxy} from '/ntp/new_tab_page_proxy.js';
import type {NtpComposeboxElement} from '/ntp/ntp_composebox.js';
import type {NtpSearchboxElement} from '/ntp/ntp_searchbox.js';
import {WindowProxy} from '/ntp/window_proxy.js';
import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {GlifAnimationState} from 'chrome://resources/cr_components/composebox/common.js';
import type {ComposeboxState} from 'chrome://resources/cr_components/composebox/common.js';
import type {ComposeboxVoiceSearchElement, VoicePermissionPromptState} from 'chrome://resources/cr_components/composebox/composebox_voice_search.js';
import type {SearchAnimatedGlowElement} from 'chrome://resources/cr_components/search/animated_glow.js';
import {SearchboxBrowserProxy} from 'chrome://resources/cr_components/searchbox/searchbox_browser_proxy.js';
import type {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {hexColorToSkColor, skColorToRgba} from 'chrome://resources/js/color_utils.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {FuseboxAction} from 'chrome://resources/mojo/components/omnibox/browser/fusebox_action.mojom-webui.js';
import {SearchboxOverride} from 'chrome://resources/mojo/components/omnibox/browser/fusebox_action.mojom-webui.js';
import type {PageCallbackRouter as SearchboxPageCallbackRouter} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {SkColor} from 'chrome://resources/mojo/skia/public/mojom/skcolor.mojom-webui.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';

// Same breakpoint as chrome://new-tab-page: 900px ~= 561px (max value for
// --ntp-search-box-width) * 1.5 + some margin.
const realboxCanShowSecondarySideMediaQueryList =
    window.matchMedia('(min-width: 900px)');

// Same voice search parameters as chrome://new-tab-page.
const VOICE_QUERY_LENGTH_LIMIT = 120;
const VOICE_AUTO_SUBMIT_IDLE_TIMEOUT_MS = 3000;
const VOICE_MANUAL_SUBMIT_IDLE_TIMEOUT_MS = 10000;
const PERMISSION_PROMPT_CSS_CLASS = 'permission-prompt-showing';


const NtpExtensionAppElementBase = I18nMixinLit(CrLitElement);

// A minimal New Tab Page hosted in a component extension. Reuses the
// <ntp-logo> (Google logo / doodle), <ntp-searchbox> (realbox),
// <ntp-composebox>, <ntp-action-chips> and <ntp-lens-upload-dialog> elements
// from chrome://new-tab-page. The realbox / composebox / action chips / voice
// search / Lens orchestration below mirrors
// chrome/browser/resources/new_tab_page/app.ts; keep the two in sync.
export class NtpExtensionAppElement extends NtpExtensionAppElementBase {
  static get is() {
    return 'ntp-extension-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      realboxCanShowSecondarySide: {type: Boolean, reflect: true},
      theme_: {type: Object},
      colorSourceIsBaseline_: {type: Boolean},
      realboxShown_: {type: Boolean},
      composeButtonEnabled_: {type: Boolean},
      composeboxEnabled_: {type: Boolean},
      composeboxState_: {type: Object},
      showComposebox_: {type: Boolean, reflect: true},
      caretAnimationsEnabled_: {type: Boolean},
      usePecApi_: {type: Boolean},
      smartTabSharingVisible_: {type: Boolean},
      contextManagementInComposeboxEnabled_: {type: Boolean},
      multiLineEnabled_: {type: Boolean},
      ntpRealboxNextEnabled_: {type: Boolean, reflect: true},
      searchboxCyclingPlaceholders_: {type: Boolean},
      ntpNextFeaturesEnabled_: {type: Boolean},
      isActionChipsVisible_: {type: Boolean},
      containerFocused_: {type: Boolean},
      showScrim_: {type: Boolean, reflect: true},
      undoToastMessage_: {type: String},
      showLensUploadDialog_: {type: Boolean},
      showVoiceSearch_: {type: Boolean, reflect: true},
      hasVoiceSearchError_: {type: Boolean},
      voiceSearchTranscript_: {type: String},
      voiceSearchReceivedSpeech_: {type: Boolean},
      voiceSearchListening_: {type: Boolean},
      voiceSearchLiveTranscriptionEnabled_: {type: Boolean},
      voiceSearchAutoEndpointEnabled_: {type: Boolean},
      voiceSearchHelperTextEnabled_: {type: Boolean},
      androidSpeechRecognition_: {type: Boolean},
      voiceQueryLengthLimit_: {type: Number},
      voiceAutoSubmitIdleTimeoutMs_: {type: Number},
      voiceManualSubmitIdleTimeoutMs_: {type: Number},
      searchboxCallbackRouter_: {type: Object},
      contextMenuGlifAnimationState_: {type: String},
      energyEffectEnabled_: {type: Boolean, reflect: true},
      energyEffectAnimationEnabled_: {type: Boolean, reflect: true},
      energyEffectVariant_: {type: String, reflect: true},
    };
  }

  accessor realboxCanShowSecondarySide: boolean =
      realboxCanShowSecondarySideMediaQueryList.matches;

  protected accessor theme_: Theme|null = null;
  protected accessor colorSourceIsBaseline_: boolean = false;
  protected accessor realboxShown_: boolean = false;
  protected accessor composeButtonEnabled_: boolean =
      loadTimeData.getBoolean('searchboxShowComposeEntrypoint');
  protected accessor composeboxEnabled_: boolean =
      loadTimeData.getBoolean('searchboxShowComposebox');
  protected accessor composeboxState_: ComposeboxState|null = null;
  protected accessor showComposebox_: boolean = false;
  protected accessor caretAnimationsEnabled_: boolean =
      loadTimeData.getBoolean('caretAnimationEnabled');
  protected accessor usePecApi_: boolean =
      loadTimeData.getBoolean('contextualMenuUsePecApi');
  protected accessor smartTabSharingVisible_: boolean =
      loadTimeData.getBoolean('composeboxSmartTabSharingVisible');
  protected accessor contextManagementInComposeboxEnabled_: boolean =
      loadTimeData.getBoolean('contextManagementInComposeboxEnabled');
  protected accessor multiLineEnabled_: boolean =
      loadTimeData.getBoolean('multiLineEnabled');
  protected accessor ntpRealboxNextEnabled_: boolean =
      loadTimeData.getBoolean('ntpRealboxNextEnabled');
  protected accessor searchboxCyclingPlaceholders_: boolean =
      loadTimeData.getBoolean('searchboxCyclingPlaceholders');
  protected accessor ntpNextFeaturesEnabled_: boolean =
      loadTimeData.getBoolean('ntpNextFeaturesEnabled');
  protected accessor isActionChipsVisible_: boolean =
      loadTimeData.getBoolean('actionChipsEnabled');
  protected accessor containerFocused_: boolean = false;
  protected accessor showScrim_: boolean = false;
  protected accessor undoToastMessage_: string = '';
  protected accessor showLensUploadDialog_: boolean = false;
  protected accessor showVoiceSearch_: boolean = false;
  protected accessor hasVoiceSearchError_: boolean = false;
  protected accessor voiceSearchTranscript_: string = '';
  protected accessor voiceSearchReceivedSpeech_: boolean = false;
  protected accessor voiceSearchListening_: boolean = false;
  protected accessor voiceSearchLiveTranscriptionEnabled_: boolean =
      loadTimeData.getBoolean(
          'voiceSearchCoherenceSearchboxWithLiveTranscriptionEnabled');
  protected accessor voiceSearchAutoEndpointEnabled_: boolean =
      loadTimeData.getBoolean('voiceSearchCoherenceRealboxAutoEndpointEnabled');
  protected accessor voiceSearchHelperTextEnabled_: boolean =
      loadTimeData.getBoolean('voiceSearchCoherenceRealboxHelperTextEnabled');
  // `true` only on Android, where voice search uses Android's speech
  // recognizer.
  protected accessor androidSpeechRecognition_: boolean =
      loadTimeData.getBoolean('androidSpeechRecognition');
  protected accessor voiceQueryLengthLimit_: number = VOICE_QUERY_LENGTH_LIMIT;
  protected accessor voiceAutoSubmitIdleTimeoutMs_: number =
      VOICE_AUTO_SUBMIT_IDLE_TIMEOUT_MS;
  protected accessor voiceManualSubmitIdleTimeoutMs_: number =
      VOICE_MANUAL_SUBMIT_IDLE_TIMEOUT_MS;
  protected accessor searchboxCallbackRouter_: SearchboxPageCallbackRouter;
  protected accessor contextMenuGlifAnimationState_: GlifAnimationState =
      GlifAnimationState.INELIGIBLE;
  protected accessor energyEffectEnabled_: boolean =
      loadTimeData.getBoolean('energyEffectEnabled');
  protected accessor energyEffectAnimationEnabled_: boolean =
      loadTimeData.getBoolean('energyEffectAnimationEnabled');
  protected accessor energyEffectVariant_: string =
      loadTimeData.getString('energyEffectVariant');

  private realboxContextMenuAnimationAllowed_: boolean = false;
  private contextMenuAnimationLimitingEnabled_: boolean =
      loadTimeData.getBoolean('contextMenuAnimationLimitingEnabled');
  private callbackRouter_: PageCallbackRouter;
  private pageHandler_: PageHandlerRemote;
  private eventTracker_: EventTracker = new EventTracker();
  private setThemeListenerId_: number|null = null;
  private setActionChipsVisibilityListenerId_: number|null = null;
  private undoToastCallback_: (() => void)|null = null;
  private voiceSearchActivatedByKeyboard_: boolean = false;

  constructor() {
    super();
    this.callbackRouter_ = NewTabPageProxy.getInstance().callbackRouter;
    this.pageHandler_ = NewTabPageProxy.getInstance().handler;
    this.searchboxCallbackRouter_ =
        SearchboxBrowserProxy.getInstance().callbackRouter;
  }

  override connectedCallback() {
    super.connectedCallback();
    ColorChangeUpdater.forDocument().start();
    this.setThemeListenerId_ =
        this.callbackRouter_.setTheme.addListener((theme: Theme) => {
          this.theme_ = theme;
        });
    this.setActionChipsVisibilityListenerId_ =
        this.callbackRouter_.setActionChipsVisibility.addListener(
            (isVisible: boolean) => this.isActionChipsVisible_ = isVisible);
    this.eventTracker_.add(
        realboxCanShowSecondarySideMediaQueryList, 'change',
        (e: MediaQueryListEvent) => {
          this.realboxCanShowSecondarySide = e.matches;
        });
    this.eventTracker_.add(
        window, 'keydown', (e: KeyboardEvent) => this.onWindowKeydown_(e));
    if (this.composeButtonEnabled_) {
      recordBoolean('NewTabPage.ComposeEntrypoint.Shown', true);
      this.pageHandler_.incrementComposeButtonShownCount();
    }
    this.initializeContextMenuAnimationState_();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    for (const id
             of [this.setThemeListenerId_,
                 this.setActionChipsVisibilityListenerId_]) {
      if (id !== null) {
        this.callbackRouter_.removeListener(id);
      }
    }
    this.setThemeListenerId_ = null;
    this.setActionChipsVisibilityListenerId_ = null;
    this.eventTracker_.removeAll();
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('theme_')) {
      this.colorSourceIsBaseline_ = !!this.theme_?.isBaseline;
    }

    // Do not show the realbox while the composebox or the Lens upload dialog
    // is showing.
    this.realboxShown_ =
        !!this.theme_ && !this.showComposebox_ && !this.showLensUploadDialog_;

    if (this.ntpRealboxNextEnabled_ && [
          'showComposebox_',
          'showLensUploadDialog_',
          'containerFocused_',
          'showVoiceSearch_',
        ].some(prop => changedPrivateProperties.has(prop))) {
      // As on chrome://new-tab-page, the scrim is shown while focus is within
      // the searchbox container, and kept while the composebox, the Lens
      // upload dialog or voice search is open so that a click outside of them
      // reaches the scrim's click handler.
      this.showScrim_ = this.showComposebox_ || this.showLensUploadDialog_ ||
          this.containerFocused_ || this.showVoiceSearch_;
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('theme_') && this.theme_) {
      document.body.style.backgroundColor =
          skColorToRgba(this.theme_.backgroundColor);
      const logoColor = this.getLogoColor_();
      if (logoColor) {
        this.style.setProperty('--ntp-logo-color', skColorToRgba(logoColor));
      } else {
        this.style.removeProperty('--ntp-logo-color');
      }
    }

    if (changedPrivateProperties.has('showVoiceSearch_') &&
        this.showVoiceSearch_) {
      this.voiceSearchListening_ = true;
      this.voiceSearchReceivedSpeech_ = false;
      this.voiceSearchTranscript_ = '';

      const dialog = this.getVoiceSearchDialog_();
      assert(dialog);
      dialog.showModal();
      dialog.focus();

      const voiceSearch = this.getVoiceSearch_();
      assert(voiceSearch);
      voiceSearch.activatedByKeyboard = this.voiceSearchActivatedByKeyboard_;
      voiceSearch.start();
    }
  }

  protected onActionChipClick_(e: CustomEvent<ActionChipClickDetail>) {
    if (!this.composeboxEnabled_) {
      // Enforce Composebox availability at the receiver even if an event
      // bypasses the Action Chips render gate.
      return;
    }
    const detail = e.detail;
    if (this.isUnsupportedSearchboxSurface_(detail.fuseboxAction)) {
      return;
    }
    this.pageHandler_.onContextualSearchIPHEngaged();
    if (this.hasRealboxOverride_(detail.fuseboxAction)) {
      this.getSearchbox_().handleFuseboxAction(detail.fuseboxAction);
      return;
    }
    this.openComposeboxForActionChip_(detail);
  }

  protected onActionChipsRetrievalStateChanged_(
      e: CustomEvent<{state: ActionChipsRetrievalState}>) {
    if (!this.realboxContextMenuAnimationAllowed_) {
      this.contextMenuGlifAnimationState_ = GlifAnimationState.INELIGIBLE;
      return;
    }
    if (this.energyEffectAnimationEnabled_) {
      return;
    }
    // REQUESTED => SPINNER_ONLY, UPDATED => STARTED. Never go back from (or
    // restart) STARTED / FINISHED. See chrome://new-tab-page's app.ts.
    if ([GlifAnimationState.STARTED, GlifAnimationState.FINISHED].every(
            s => s !== this.contextMenuGlifAnimationState_)) {
      const state = e.detail.state;
      if (state === ActionChipsRetrievalState.REQUESTED) {
        this.contextMenuGlifAnimationState_ = GlifAnimationState.SPINNER_ONLY;
      } else if (state === ActionChipsRetrievalState.UPDATED) {
        this.contextMenuGlifAnimationState_ = GlifAnimationState.STARTED;
        this.pageHandler_.recordRealboxContextMenuAnimationImpression(true);
      }
    }
  }

  protected onContextMenuEntrypointClick_() {
    this.pageHandler_.onContextualSearchIPHEngaged();
  }

  protected onOpenComposebox_(e: CustomEvent<ComposeboxState>) {
    this.composeboxState_ = e.detail;
    this.ensureComposeboxOpen_();
  }

  protected onOpenLensSearch_() {
    this.showLensUploadDialog_ = true;
  }

  protected onCloseLensSearch_() {
    this.showLensUploadDialog_ = false;
  }

  // Voice search always uses the inline <cr-composebox-voice-search>, which
  // logs the voice search metrics, as does the composebox's voice search.
  // Unlike chrome://new-tab-page, the extension doesn't fall back to
  // <ntp-voice-search-overlay> or log the legacy NewTabPage.VoiceActions
  // metrics itself when the voice search coherence experiment is disabled.
  protected onOpenVoiceSearch_() {
    this.openVoiceSearch_(/*activatedByKeyboard=*/ false);
  }

  protected onVoiceSearchClose_() {
    const dialog = this.getVoiceSearchDialog_();
    if (dialog && dialog.open) {
      return;
    }
    this.showVoiceSearch_ = false;
    this.hasVoiceSearchError_ = false;
    this.voiceSearchListening_ = false;
  }

  protected onVoiceSearchDialogClick_(e: MouseEvent) {
    const dialog = e.currentTarget as HTMLDialogElement;
    if (e.target === dialog) {
      dialog.close();
    }
  }

  protected onVoicePermissionChanged_(
      e: CustomEvent<VoicePermissionPromptState>) {
    const isOpened = e.detail.isOpened;
    this.voiceSearchListening_ =
        !isOpened && this.showVoiceSearch_ && !this.hasVoiceSearchError_;
    const glow = this.shadowRoot.querySelector<SearchAnimatedGlowElement>(
        '#voiceSearchGlow');
    glow?.classList.toggle(PERMISSION_PROMPT_CSS_CLASS, isOpened);
    this.getVoiceSearch_()?.classList.toggle(
        PERMISSION_PROMPT_CSS_CLASS, isOpened);
  }

  protected onVoiceSearchCancel_() {
    this.closeVoiceSearch_();
  }

  protected onVoiceSearchError_() {
    if (this.showVoiceSearch_) {
      this.hasVoiceSearchError_ = true;
    }
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

  protected onVoiceSearchFinalResult_(e: CustomEvent<string>) {
    this.closeVoiceSearch_();
    const searchParams = new URLSearchParams();
    searchParams.append('q', e.detail);
    searchParams.append('gs_ivs', '1');
    searchParams.append('sourceid', 'chrome');
    const queryUrl =
        new URL('/search', loadTimeData.getString('googleBaseUrl'));
    queryUrl.search = searchParams.toString();
    WindowProxy.getInstance().navigate(queryUrl.href);
  }

  protected onVoiceSearchRecordingStopped_(e: CustomEvent<string>) {
    this.closeVoiceSearch_();
    const query = e.detail;
    if (query && query.trim().length > 0) {
      const searchbox = this.getSearchbox_();
      searchbox.setInputText(query);
      searchbox.focusInput();
      searchbox.queryAutocomplete(query, false, false);
    }
  }

  protected showVoiceSearchGlow_(): boolean {
    if (this.hasVoiceSearchError_) {
      return false;
    }
    // As on chrome://new-tab-page.
    return !this.voiceSearchLiveTranscriptionEnabled_ ||
        this.voiceSearchHelperTextEnabled_;
  }

  protected onActionChipsDisabled_(
      e: CustomEvent<{message: string, undo: () => void}>) {
    this.undoToastCallback_ = e.detail.undo;
    this.undoToastMessage_ = e.detail.message;
    this.shadowRoot.querySelector<CrToastElement>('#undoToast')!.show();
  }

  protected onUndoButtonClick_() {
    this.undoToastCallback_?.();
    this.undoToastCallback_ = null;
    this.shadowRoot.querySelector<CrToastElement>('#undoToast')!.hide();
  }

  protected onComposeboxContextMenuOpened_() {
    this.getSearchbox_().closeContextMenu();
  }

  protected onCloseComposebox_(e: CustomEvent<{composeboxText?: string}>) {
    if (!this.showComposebox_) {
      return;
    }
    const searchbox = this.getSearchbox_();
    searchbox.closeContextMenu();
    // Carry any text typed in the composebox back to the realbox.
    const composeboxText = e.detail.composeboxText;
    if (composeboxText && composeboxText.trim()) {
      searchbox.setInputText(composeboxText);
    }
    const composebox = this.getComposebox_();
    assert(composebox);
    composebox.input = '';
    composebox.resetModes();
    if (this.ntpRealboxNextEnabled_) {
      composebox.closeDropdown();
    }
    this.showComposebox_ = false;
  }

  protected onScrimClick_() {
    if (this.showComposebox_) {
      const composebox = this.getComposebox_();
      assert(composebox);
      this.onCloseComposebox_(new CustomEvent(
          'close-composebox', {detail: {composeboxText: composebox.input}}));
    }
    if (this.showLensUploadDialog_) {
      this.onCloseLensSearch_();
    }
    if (this.showVoiceSearch_) {
      const dialog = this.getVoiceSearchDialog_();
      if (dialog && dialog.open) {
        dialog.close();
      }
    }
    this.containerFocused_ = false;
  }

  protected onSearchboxContainerFocusin_() {
    if (this.ntpRealboxNextEnabled_) {
      this.containerFocused_ = true;
    }
  }

  protected onSearchboxContainerFocusout_() {
    if (this.ntpRealboxNextEnabled_) {
      this.containerFocused_ =
          this.shadowRoot.getElementById('searchboxContainer')!.matches(
              ':focus-within');
    }
  }

  // As on chrome://new-tab-page, the logo is single-colored, white by default,
  // in dark mode.
  protected isLogoSingleColored_(): boolean {
    return !!this.theme_ && (!!this.theme_.logoColor || this.theme_.isDark);
  }

  private getLogoColor_(): SkColor|null {
    if (!this.theme_) {
      return null;
    }
    return this.theme_.logoColor ||
        (this.theme_.isDark ? hexColorToSkColor('#ffffff') : null);
  }

  protected isThemeDark_(): boolean {
    return !!this.theme_?.isDark;
  }

  protected showActionChips_(): boolean {
    return this.ntpNextFeaturesEnabled_ && this.isActionChipsVisible_ &&
        this.composeboxEnabled_;
  }

  protected showActionChipsBackground_(): boolean {
    return !!this.theme_ &&
        (!!this.theme_.backgroundImage || !this.theme_.isGm3);
  }

  private async openComposeboxForActionChip_(detail: ActionChipClickDetail) {
    this.composeboxState_ = null;
    this.ensureComposeboxOpen_();
    await this.updateComplete;
    const composebox = this.getComposebox_();
    if (composebox) {
      await composebox.handleFuseboxAction(detail);
    }
  }

  // The Composebox is the only searchbox surface supported for action chip
  // clicks; an explicit request for a different surface is a no-op. An action
  // with the override missing keeps the compatible open-Composebox behavior.
  private isUnsupportedSearchboxSurface_(action?: FuseboxAction): boolean {
    if (!action || action.searchboxOverride === null) {
      return false;
    }
    return action.searchboxOverride === SearchboxOverride.kUnspecified;
  }

  private hasRealboxOverride_(action?: FuseboxAction): boolean {
    return !!action && action.searchboxOverride === SearchboxOverride.kRealbox;
  }

  // Opens the composebox, or focuses its input if it is already open.
  private ensureComposeboxOpen_() {
    if (!this.showComposebox_) {
      this.showComposebox_ = true;
    } else {
      this.getComposebox_()?.focusInput();
    }
  }

  private async initializeContextMenuAnimationState_() {
    if (!this.ntpRealboxNextEnabled_) {
      this.realboxContextMenuAnimationAllowed_ = false;
      this.contextMenuGlifAnimationState_ = GlifAnimationState.INELIGIBLE;
      return;
    }
    let canShow = true;
    if (this.contextMenuAnimationLimitingEnabled_) {
      ({canShow} =
           await this.pageHandler_.canShowRealboxContextMenuAnimation());
    }
    this.realboxContextMenuAnimationAllowed_ = canShow;
    if (!canShow) {
      this.contextMenuGlifAnimationState_ = GlifAnimationState.INELIGIBLE;
      this.pageHandler_.recordRealboxContextMenuAnimationImpression(false);
      return;
    }
    if (this.energyEffectAnimationEnabled_) {
      this.contextMenuGlifAnimationState_ = GlifAnimationState.STARTED;
      this.pageHandler_.recordRealboxContextMenuAnimationImpression(true);
      return;
    }
    // Otherwise the animation is driven by the action chips retrieval state.
    this.contextMenuGlifAnimationState_ =
        this.ntpNextFeaturesEnabled_ && this.isActionChipsVisible_ ?
        GlifAnimationState.SPINNER_ONLY :
        GlifAnimationState.INELIGIBLE;
  }

  private onWindowKeydown_(e: KeyboardEvent) {
    if (e.key === 'Escape' && this.showComposebox_) {
      const composebox = this.getComposebox_();
      if (composebox) {
        composebox.handleEscapeKeyLogic();
        e.preventDefault();
        return;
      }
    }
    let ctrlKeyPressed = e.ctrlKey;
    // <if expr="is_macosx">
    ctrlKeyPressed = ctrlKeyPressed || e.metaKey;
    // </if>
    // Ctrl/Cmd+Shift+. opens voice search, as on chrome://new-tab-page.
    if (ctrlKeyPressed && e.code === 'Period' && e.shiftKey) {
      this.openVoiceSearch_(/*activatedByKeyboard=*/ true);
    }
  }

  private openVoiceSearch_(activatedByKeyboard: boolean) {
    this.voiceSearchActivatedByKeyboard_ = activatedByKeyboard;
    this.showVoiceSearch_ = true;
  }

  private closeVoiceSearch_() {
    this.getVoiceSearchDialog_()?.close();
    this.onVoiceSearchClose_();
  }

  private getVoiceSearchDialog_(): HTMLDialogElement|null {
    return this.shadowRoot.querySelector<HTMLDialogElement>(
        '#voiceSearchDialog');
  }

  private getVoiceSearch_(): ComposeboxVoiceSearchElement|null {
    return this.shadowRoot.querySelector<ComposeboxVoiceSearchElement>(
        '#voiceSearch');
  }

  private getSearchbox_(): NtpSearchboxElement {
    const searchbox =
        this.shadowRoot.querySelector<NtpSearchboxElement>('#searchbox');
    assert(searchbox);
    return searchbox;
  }

  private getComposebox_(): NtpComposeboxElement|null {
    return this.shadowRoot.querySelector<NtpComposeboxElement>('#composebox');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ntp-extension-app': NtpExtensionAppElement;
  }
}

customElements.define(NtpExtensionAppElement.is, NtpExtensionAppElement);
