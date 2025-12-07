// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './read_anything_toolbar.js';
import '/strings.m.js';
import '//read-anything-side-panel.top-chrome/shared/sp_empty_state.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_toast/cr_toast.js';
import '../read_aloud/language_toast.js';

import {ColorChangeUpdater} from '//resources/cr_components/color_change_listener/colors_css_updater.js';
import {WebUiListenerMixinLit} from '//resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {ContentController, ContentType} from '../content/content_controller.js';
import type {ContentListener, ContentState} from '../content/content_controller.js';
import {NodeStore} from '../content/node_store.js';
import {SelectionController} from '../content/selection_controller.js';
import type {LanguageToastElement} from '../read_aloud/language_toast.js';
import {SpeechController} from '../read_aloud/speech_controller.js';
import type {SpeechListener} from '../read_aloud/speech_controller.js';
import {TextSegmenter} from '../read_aloud/text_segmenter.js';
import {VoiceLanguageController} from '../read_aloud/voice_language_controller.js';
import type {VoiceLanguageListener} from '../read_aloud/voice_language_controller.js';
import {VoiceNotificationManager} from '../read_aloud/voice_notification_manager.js';
import type {SettingsPrefs} from '../shared/common.js';
import {getWordCount, minOverflowLengthToScroll} from '../shared/common.js';
import {ReadAnythingLogger, TimeFrom} from '../shared/read_anything_logger.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {AppStyleUpdater} from './app_style_updater.js';
import type {ReadAnythingToolbarElement} from './read_anything_toolbar.js';

const AppElementBase = WebUiListenerMixinLit(CrLitElement);

export interface AppElement {
  $: {
    toolbar: ReadAnythingToolbarElement,
    appFlexParent: HTMLElement,
    container: HTMLElement,
    languageToast: LanguageToastElement,
    containerScroller: HTMLElement,
  };
}

export class AppElement extends AppElementBase implements SpeechListener,
                                                          VoiceLanguageListener,
                                                          ContentListener {
  static get is() {
    return 'read-anything-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      isSpeechActive_: {type: Boolean},
      isAudioCurrentlyPlaying_: {type: Boolean},
      enabledLangs_: {type: Array},
      settingsPrefs_: {type: Object},
      selectedVoice_: {type: Object},
      availableVoices_: {type: Array},
      previewVoicePlaying_: {type: Object},
      localeToDisplayName_: {type: Object},
      contentState_: {type: Object},
      speechEngineLoaded_: {type: Boolean},
      willDrawAgainSoon_: {type: Boolean},
      pageLanguage_: {type: String},
    };
  }

  private startTime_ = Date.now();

  protected accessor contentState_: ContentState;

  private isReadAloudEnabled_: boolean;
  protected isDocsLoadMoreButtonVisible_: boolean = false;

  // If the speech engine is considered "loaded." If it is, we should display
  // the play / pause buttons normally. Otherwise, we should disable the
  // Read Aloud controls until the engine has loaded in order to provide
  // visual feedback that a voice is about to be spoken.
  private accessor speechEngineLoaded_: boolean = true;

  // Sometimes distillations are queued up while distillation is happening so
  // when the current distillation finishes, we re-distill immediately. In that
  // case we shouldn't allow playing speech until the next distillation to avoid
  // resetting speech right after starting it.
  private accessor willDrawAgainSoon_: boolean = false;

  protected accessor selectedVoice_: SpeechSynthesisVoice|null = null;
  // The set of languages currently enabled for use by Read Aloud. This
  // includes user-enabled languages and auto-downloaded languages. The former
  // are stored in preferences. The latter are not.
  protected accessor enabledLangs_: string[] = [];

  // All possible available voices for the current speech engine.
  protected accessor availableVoices_: SpeechSynthesisVoice[] = [];
  // If a preview is playing, this is set to the voice the preview is playing.
  // Otherwise, this is null.
  protected accessor previewVoicePlaying_: SpeechSynthesisVoice|null = null;

  protected accessor localeToDisplayName_: {[locale: string]: string} = {};
  protected accessor pageLanguage_: string = '';

  private notificationManager_ = VoiceNotificationManager.getInstance();
  private logger_: ReadAnythingLogger = ReadAnythingLogger.getInstance();
  private styleUpdater_: AppStyleUpdater;
  private nodeStore_: NodeStore = NodeStore.getInstance();
  private voiceLanguageController_: VoiceLanguageController =
      VoiceLanguageController.getInstance();
  private speechController_: SpeechController = SpeechController.getInstance();
  private contentController_: ContentController =
      ContentController.getInstance();
  private selectionController_: SelectionController =
      SelectionController.getInstance();
  protected accessor settingsPrefs_: SettingsPrefs = {
    letterSpacing: 0,
    lineSpacing: 0,
    theme: 0,
    speechRate: 0,
    font: '',
    highlightGranularity: 0,
  };

  protected accessor isSpeechActive_: boolean = false;
  protected accessor isAudioCurrentlyPlaying_: boolean = false;

  constructor() {
    super();
    this.logger_.logTimeFrom(TimeFrom.APP, this.startTime_, Date.now());
    this.isReadAloudEnabled_ = chrome.readingMode.isReadAloudEnabled;
    this.styleUpdater_ = new AppStyleUpdater(this);
    this.nodeStore_.clear();
    ColorChangeUpdater.forDocument().start();
    TextSegmenter.getInstance().updateLanguage(
        chrome.readingMode.baseLanguageForSpeech);
    this.contentState_ = this.contentController_.getState();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    // Even though disconnectedCallback isn't always called reliably in prod,
    // it is called in tests, and the speech extension timeout can cause
    // flakiness.
    this.voiceLanguageController_.stopWaitingForSpeechExtension();
  }

  override connectedCallback() {
    super.connectedCallback();

    // onConnected should always be called first in connectedCallback to ensure
    // we're not blocking onConnected on anything else during WebUI setup.
    if (chrome.readingMode) {
      chrome.readingMode.onConnected();
    }

    // Push ShowUI() callback to the event queue to allow deferred rendering
    // to take place.
    setTimeout(() => chrome.readingMode.shouldShowUi(), 0);
    this.styleUpdater_.setMaxLineWidth();

    this.contentController_.addListener(this);
    if (this.isReadAloudEnabled_) {
      this.speechController_.addListener(this);
      this.voiceLanguageController_.addListener(this);
      this.notificationManager_.addListener(this.$.languageToast);

      // Clear state. We don't do this in disconnectedCallback because that's
      // not always reliabled called.
      this.nodeStore_.clearDomNodes();
    }
    this.showLoading();

    this.settingsPrefs_ = {
      letterSpacing: chrome.readingMode.letterSpacing,
      lineSpacing: chrome.readingMode.lineSpacing,
      theme: chrome.readingMode.colorTheme,
      speechRate: chrome.readingMode.speechRate,
      font: chrome.readingMode.fontName,
      highlightGranularity: chrome.readingMode.highlightGranularity,
    };

    document.onselectionchange = () => {
      // When Read Aloud is playing, user-selection is disabled on the Read
      // Anything panel, so don't attempt to update selection, as this can
      // end up clearing selection in the main part of the browser.
      if (!this.contentController_.hasContent() ||
          this.speechController_.isSpeechActive()) {
        return;
      }

      const selection = this.getSelection();
      this.selectionController_.onSelectionChange(selection);
      if (this.isReadAloudEnabled_) {
        this.speechController_.onSelectionChange();
        this.contentController_.onSelectionChange(this.shadowRoot);
      }
    };

    // Pass copy commands to main page. Copy commands will not work if they are
    // disabled on the main page.
    document.oncopy = () => {
      chrome.readingMode.onCopy();
      return false;
    };

    /////////////////////////////////////////////////////////////////////
    // Called by ReadAnythingUntrustedPageHandler via callback router. //
    /////////////////////////////////////////////////////////////////////
    chrome.readingMode.updateContent = () => {
      this.updateContent();
    };

    chrome.readingMode.updateLinks = () => {
      this.updateLinks_();
    };

    chrome.readingMode.updateImages = () => {
      this.updateImages_();
    };

    chrome.readingMode.onImageDownloaded = (nodeId) => {
      this.contentController_.onImageDownloaded(nodeId);
    };

    chrome.readingMode.updateSelection = () => {
      this.selectionController_.updateSelection(
          this.getSelection(), this.$.container);
    };

    chrome.readingMode.updateVoicePackStatus =
        (lang: string, status: string) => {
          this.voiceLanguageController_.updateLanguageStatus(lang, status);
        };

    chrome.readingMode.showLoading = () => {
      this.showLoading();
    };

    chrome.readingMode.showEmpty = () => {
      this.contentController_.setEmpty();
    };

    chrome.readingMode.restoreSettingsFromPrefs = () => {
      this.restoreSettingsFromPrefs_();
    };

    chrome.readingMode.languageChanged = () => {
      this.languageChanged();
    };

    chrome.readingMode.onLockScreen = () => {
      this.speechController_.onLockScreen();
    };

    chrome.readingMode.onTtsEngineInstalled = () => {
      this.voiceLanguageController_.onTtsEngineInstalled();
    };

    chrome.readingMode.onTabMuteStateChange = (muted: boolean) => {
      this.speechController_.onTabMuteStateChange(muted);
    };

    chrome.readingMode.onNodeWillBeDeleted = (nodeId: number) => {
      this.contentController_.onNodeWillBeDeleted(nodeId);
    };
  }

  protected onContainerScroll_() {
    this.selectionController_.onScroll();
    if (this.isReadAloudEnabled_) {
      this.speechController_.onScroll();
    }
  }

  protected onContainerScrollEnd_() {
    this.nodeStore_.estimateWordsSeenWithDelay();
  }

  showLoading() {
    this.contentController_.setState(ContentType.LOADING);
    if (this.isReadAloudEnabled_) {
      this.speechController_.resetForNewContent();
    }
  }

  // TODO: crbug.com/40927698 - Handle focus changes for speech, including
  // updating speech state.
  updateContent() {
    this.willDrawAgainSoon_ = chrome.readingMode.requiresDistillation;
    this.isDocsLoadMoreButtonVisible_ =
        chrome.readingMode.isDocsLoadMoreButtonVisible;

    // Remove all children from container. Use `replaceChildren` rather than
    // setting `innerHTML = ''` in order to remove all listeners, too.
    this.$.container.replaceChildren();
    const newRoot = this.contentController_.updateContent();
    if (newRoot) {
      this.$.container.appendChild(newRoot);
    }
    if (!this.willDrawAgainSoon_) {
      const wordCount = (newRoot && newRoot.textContent) ?
          getWordCount(newRoot.textContent) :
          0;
      chrome.readingMode.onDistilled(wordCount);
    }
  }

  getSelection(): Selection|null {
    assert(this.shadowRoot, 'no shadow root');
    return this.shadowRoot.getSelection();
  }

  protected updateLinks_() {
    this.contentController_.updateLinks(this.shadowRoot);
  }

  protected updateImages_() {
    this.contentController_.updateImages(this.shadowRoot);
  }

  protected onDocsLoadMoreButtonClick_() {
    chrome.readingMode.onScrolledToBottom();
  }

  protected onLanguageMenuOpen_() {
    this.notificationManager_.removeListener(this.$.languageToast);
  }

  protected onLanguageMenuClose_() {
    this.notificationManager_.addListener(this.$.languageToast);
  }

  protected onPreviewVoice_(
      event: CustomEvent<{previewVoice: SpeechSynthesisVoice}>) {
    event.preventDefault();
    event.stopPropagation();

    this.speechController_.previewVoice(event.detail.previewVoice);
  }

  protected onVoiceMenuOpen_(event: CustomEvent) {
    event.preventDefault();
    event.stopPropagation();
    this.speechController_.onVoiceMenuOpen();
  }

  protected onVoiceMenuClose_(event: CustomEvent) {
    event.preventDefault();
    event.stopPropagation();
    this.speechController_.onVoiceMenuClose();
  }

  protected onPlayPauseClick_() {
    this.speechController_.onPlayPauseToggle(this.$.container);
  }

  onContentStateChange(): void {
    this.contentState_ = this.contentController_.getState();
  }

  onNewPageDrawn(): void {
    this.$.containerScroller.scrollTop = 0;
  }

  onPlayingFromSelection(): void {
    // Clear the selection so we don't keep trying to play from the same
    // selection every time they press play.
    this.getSelection()?.removeAllRanges();
  }

  onIsSpeechActiveChange(): void {
    this.isSpeechActive_ = this.speechController_.isSpeechActive();
    if (chrome.readingMode.linksEnabled &&
        !this.speechController_.isTemporaryPause()) {
      this.updateLinks_();
    }
  }

  onIsAudioCurrentlyPlayingChange(): void {
    this.isAudioCurrentlyPlaying_ =
        this.speechController_.isAudioCurrentlyPlaying();
  }

  onEngineStateChange(): void {
    this.speechEngineLoaded_ = this.speechController_.isEngineLoaded();
  }

  onPreviewVoicePlaying(): void {
    this.previewVoicePlaying_ = this.speechController_.getPreviewVoicePlaying();
  }

  onEnabledLangsChange(): void {
    this.enabledLangs_ = this.voiceLanguageController_.getEnabledLangs();
  }

  onAvailableVoicesChange(): void {
    this.availableVoices_ = this.voiceLanguageController_.getAvailableVoices();
    this.localeToDisplayName_ =
        this.voiceLanguageController_.getDisplayNamesForLocaleCodes();
  }

  onCurrentVoiceChange(): void {
    this.selectedVoice_ = this.voiceLanguageController_.getCurrentVoice();
    this.speechController_.onSpeechSettingsChange();
  }

  protected onNextGranularityClick_() {
    this.speechController_.onNextGranularityClick();
  }

  protected onPreviousGranularityClick_() {
    this.speechController_.onPreviousGranularityClick();
  }

  protected onSelectVoice_(
      event: CustomEvent<{selectedVoice: SpeechSynthesisVoice}>) {
    event.preventDefault();
    event.stopPropagation();
    this.speechController_.onVoiceSelected(event.detail.selectedVoice);
  }

  protected onVoiceLanguageToggle_(event: CustomEvent<{language: string}>) {
    event.preventDefault();
    event.stopPropagation();
    this.voiceLanguageController_.onLanguageToggle(event.detail.language);
  }

  protected onSpeechRateChange_() {
    this.speechController_.onSpeechSettingsChange();
  }

  private restoreSettingsFromPrefs_() {
    if (this.isReadAloudEnabled_) {
      this.voiceLanguageController_.restoreFromPrefs();
    }
    this.settingsPrefs_ = {
      ...this.settingsPrefs_,
      letterSpacing: chrome.readingMode.letterSpacing,
      lineSpacing: chrome.readingMode.lineSpacing,
      theme: chrome.readingMode.colorTheme,
      speechRate: chrome.readingMode.speechRate,
      font: chrome.readingMode.fontName,
      highlightGranularity: chrome.readingMode.highlightGranularity,
    };
    this.styleUpdater_.setAllTextStyles();
    // TODO: crbug.com/40927698 - Remove this call. Using this.settingsPrefs_
    // should replace this direct call to the toolbar.
    this.$.toolbar.restoreSettingsFromPrefs();
  }

  protected onLineSpacingChange_() {
    this.styleUpdater_.setLineSpacing();
  }

  protected onLetterSpacingChange_() {
    this.styleUpdater_.setLetterSpacing();
  }

  protected onFontChange_() {
    this.styleUpdater_.setFont();
  }

  protected onFontSizeChange_() {
    this.styleUpdater_.setFontSize();
  }

  protected onThemeChange_() {
    this.styleUpdater_.setTheme();
  }

  protected onResetToolbar_() {
    this.styleUpdater_.resetToolbar();
  }

  protected onToolbarOverflow_(event: CustomEvent<{overflowLength: number}>) {
    const shouldScroll =
        (event.detail.overflowLength >= minOverflowLengthToScroll);
    this.styleUpdater_.overflowToolbar(shouldScroll);
  }

  protected onHighlightChange_(event: CustomEvent<{data: number}>) {
    this.speechController_.onHighlightGranularityChange(event.detail.data);
    // Apply highlighting changes to the DOM.
    this.styleUpdater_.setHighlight();
  }

  languageChanged() {
    this.pageLanguage_ = chrome.readingMode.baseLanguageForSpeech;
    if (this.isReadAloudEnabled_) {
      this.voiceLanguageController_.onPageLanguageChanged();
      TextSegmenter.getInstance().updateLanguage(this.pageLanguage_);
    }
  }

  protected computeHasContent(): boolean {
    return this.contentState_.type === ContentType.HAS_CONTENT;
  }

  protected computeIsReadAloudPlayable(): boolean {
    return (this.contentState_.type === ContentType.HAS_CONTENT) &&
        this.speechEngineLoaded_ && !!this.selectedVoice_ &&
        !this.willDrawAgainSoon_;
  }

  protected onKeyDown_(e: KeyboardEvent) {
    if (e.key === 'k') {
      e.stopPropagation();
      e.preventDefault();
      this.speechController_.onPlayPauseKeyPress(this.$.container);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'read-anything-app': AppElement;
  }
}

customElements.define(AppElement.is, AppElement);
