// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './read_anything_toolbar.js';
import '/strings.m.js';
import '//read-anything-side-panel.top-chrome/shared/sp_empty_state.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_toast/cr_toast.js';
import './language_toast.js';

import {ColorChangeUpdater} from '//resources/cr_components/color_change_listener/colors_css_updater.js';
import {WebUiListenerMixinLit} from '//resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {AppStyleUpdater} from './app_style_updater.js';
import type {SettingsPrefs} from './common.js';
import {LOG_EMPTY_DELAY_MS, minOverflowLengthToScroll} from './common.js';
import {ContentController} from './content_controller.js';
import type {LanguageToastElement} from './language_toast.js';
import {NodeStore} from './node_store.js';
import {getReadAloudModel} from './read_aloud/read_aloud_model_browser_proxy.js';
import {ReadAloudNode} from './read_aloud/read_aloud_types.js';
import {SpeechController} from './read_aloud/speech_controller.js';
import type {SpeechListener} from './read_aloud/speech_controller.js';
import {TextSegmenter} from './read_aloud/text_segmenter.js';
import {VoiceLanguageController} from './read_aloud/voice_language_controller.js';
import type {VoiceLanguageListener} from './read_aloud/voice_language_controller.js';
import {VoiceNotificationManager} from './read_aloud/voice_notification_manager.js';
import {ReadAnythingLogger, TimeFrom} from './read_anything_logger.js';
import type {ReadAnythingToolbarElement} from './read_anything_toolbar.js';
import {SelectionController} from './selection_controller.js';

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

export class AppElement extends AppElementBase implements
    SpeechListener, VoiceLanguageListener {
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
      hasContent_: {type: Boolean},
      speechEngineLoaded_: {type: Boolean},
      willDrawAgainSoon_: {type: Boolean},
      emptyStateImagePath_: {type: String},
      emptyStateDarkImagePath_: {type: String},
      emptyStateHeading_: {type: String},
      emptyStateSubheading_: {type: String},
    };
  }

  private startTime = Date.now();
  private constructorTime: number;

  protected accessor hasContent_ = false;
  protected accessor emptyStateImagePath_: string|undefined;
  protected accessor emptyStateDarkImagePath_: string|undefined;
  protected accessor emptyStateHeading_: string|undefined;
  protected accessor emptyStateSubheading_ = '';

  private previousRootId_?: number;

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
    this.constructorTime = Date.now();
    this.logger_.logTimeFrom(
        TimeFrom.APP, this.startTime, this.constructorTime);
    this.isReadAloudEnabled_ = chrome.readingMode.isReadAloudEnabled;
    this.styleUpdater_ = new AppStyleUpdater(this);
    this.nodeStore_.clear();
    ColorChangeUpdater.forDocument().start();
    TextSegmenter.getInstance().updateLanguage(
        chrome.readingMode.baseLanguageForSpeech);
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
    this.showLoading();

    if (this.isReadAloudEnabled_) {
      this.speechController_.addListener(this);
      this.voiceLanguageController_.addListener(this);
      this.notificationManager_.addListener(this.$.languageToast);

      // Clear state. We don't do this in disconnectedCallback because that's
      // not always reliabled called.
      this.hasContent_ = false;
      this.nodeStore_.clearDomNodes();
    }

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
      if (!this.hasContent_ || this.speechController_.isSpeechActive()) {
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
      this.selectionController_.updateSelection(this.getSelection());
    };

    chrome.readingMode.updateVoicePackStatus =
        (lang: string, status: string) => {
          this.voiceLanguageController_.updateLanguageStatus(lang, status);
        };

    chrome.readingMode.showLoading = () => {
      this.showLoading();
    };

    chrome.readingMode.showEmpty = () => {
      this.showEmpty();
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
      this.onNodeWillBeDeleted(nodeId);
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

  showEmpty() {
    if (this.isEmptyState()) {
      return;
    }
    // Log the empty state only after a short delay. Sometimes the empty state
    // is only shown very briefly before the content is distilled, so we don't
    // need to count those instances as a failure to distill.
    setTimeout(() => {
      if (this.isEmptyState() && !this.hasContent_) {
        this.logger_.logEmptyState();
      }
    }, LOG_EMPTY_DELAY_MS);
    if (!chrome.readingMode.isGoogleDocs) {
      this.emptyStateHeading_ = loadTimeData.getString('emptyStateHeader');
    } else {
      this.emptyStateHeading_ = loadTimeData.getString('notSelectableHeader');
    }
    this.emptyStateImagePath_ = './images/empty_state.svg';
    this.emptyStateDarkImagePath_ = './images/empty_state.svg';
    this.emptyStateSubheading_ = loadTimeData.getString('emptyStateSubheader');
    this.hasContent_ = false;
  }

  isEmptyState(): boolean {
    // In rare cases it is possible for hasContent_ to be false but the loading
    // screen to be shown without ever terminating, such as when reading mode
    // receives bad selection data. When this happens, reading mode needs to
    // check whether or not the empty state is currently showing, not whether
    // or not there is content.
    return this.emptyStateImagePath_ === './images/empty_state.svg';
  }

  showLoading() {
    this.emptyStateImagePath_ = '//resources/images/throbber_small.svg';
    this.emptyStateDarkImagePath_ =
        '//resources/images/throbber_small_dark.svg';
    this.emptyStateHeading_ =
        loadTimeData.getString('readAnythingLoadingMessage');
    this.emptyStateSubheading_ = '';
    this.hasContent_ = false;
    this.resetForNewContent();
  }

  // TODO: crbug.com/40927698 - Handle focus changes for speech, including
  // updating speech state.
  updateContent() {
    // This shouldn't happen. If it does, there is likely a bug, so log it so
    // we can monitor it.
    if (this.speechController_.isSpeechActive()) {
      console.error(
          'updateContent called while speech is active. ',
          'There may be a bug.');
      this.logger_.logSpeechStopSource(
          chrome.readingMode.unexpectedUpdateContentStopSource);
    }

    if (this.isReadAloudEnabled_) {
      this.speechController_.saveReadAloudState();
      this.resetForNewContent();
    }
    const container = this.$.container;

    // Remove all children from container. Use `replaceChildren` rather than
    // setting `innerHTML = ''` in order to remove all listeners, too.
    container.replaceChildren();
    this.nodeStore_.clearDomNodes();

    // Construct a dom subtree starting with the display root and append it to
    // the container. The display root may be invalid if there are no content
    // nodes and no selection.
    // This does not use Lit's templating abstraction, which would create a
    // shadow node element representing each AXNode, because experimentation
    // (with Polymer) found the shadow node creation to be ~8-10x slower than
    // constructing and appending nodes directly to the container element.
    const rootId = chrome.readingMode.rootId;
    if (!rootId) {
      return;
    }

    this.willDrawAgainSoon_ = chrome.readingMode.requiresDistillation;
    const node = this.contentController_.buildSubtree(rootId);
    // If there is no text or images in the tree, do not proceed. The empty
    // state container will show instead.
    if (!node.textContent && !this.nodeStore_.hasImagesToFetch()) {
      // Sometimes the controller thinks there will be content and redraws
      // without showing the empty page, but we end up not actually having any
      // content and also not showing the empty page sometimes. In this case,
      // send that info back to the controller.
      if (this.hasContent_) {
        this.hasContent_ = false;
        chrome.readingMode.onNoTextContent();
      } else if (!this.isEmptyState()) {
        // If no text content is found but reading mode is not showing the
        // empty state, signal back to the renderer that this is the case.
        // This is possible when the AXTree returns bad selection data and
        // reading mode believes it has selected content to distll but
        // nothing valid is selected. This can cause the loading screen
        // to never switch to the empty state.
        this.showEmpty();
      }
      return;
    }

    if (this.previousRootId_ !== rootId) {
      this.previousRootId_ = rootId;
      this.logger_.logNewPage(/*speechPlayed=*/ false);
    }

    // Always load images even if they are disabled to ensure a fast response
    // when toggling.
    this.contentController_.loadImages();

    this.isDocsLoadMoreButtonVisible_ =
        chrome.readingMode.isDocsLoadMoreButtonVisible;

    this.hasContent_ = true;
    container.appendChild(node);
    this.updateImages_();

    // If the previous reading position still exists and we haven't reached the
    // end of speech, keep that spot.
    let setPreviousReadingPosition = false;
    if (this.isReadAloudEnabled_) {
      setPreviousReadingPosition =
          this.speechController_.setPreviousReadingPositionIfExists();
    }

    requestAnimationFrame(() => {
      // Scroll back to the top after we've drawn as long as we aren't keeping
      // the reading position from before.
      if (!setPreviousReadingPosition) {
        this.$.containerScroller.scrollTop = 0;
      }
      this.nodeStore_.estimateWordsSeenWithDelay();
      // Initialize the speech tree with the new content.
      if (chrome.readingMode.isTsTextSegmentationEnabled) {
        const contextNode = ReadAloudNode.create(container);
        if (contextNode) {
          // Don't initialize until after we've drawn- otherwise, the DOM
          // nodes might not yet exist in the tree.
          getReadAloudModel().init(contextNode);
        }
      }
    });
  }

  getSelection(): Selection|null {
    assert(this.shadowRoot, 'no shadow root');
    return this.shadowRoot.getSelection();
  }

  protected resetForNewContent() {
    if (!this.isReadAloudEnabled_) {
      return;
    }

    if (chrome.readingMode.isTsTextSegmentationEnabled) {
      // Reset the read aloud model because there's new content.
      getReadAloudModel().resetModel?.();
    }

    this.speechController_.clearReadAloudState();
  }

  protected updateLinks_() {
    this.contentController_.updateLinks(this.hasContent_, this.shadowRoot);
  }

  protected updateImages_() {
    this.contentController_.updateImages(this.hasContent_, this.shadowRoot);
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
    this.speechController_.onPlayPauseToggle(
        this.getSelection(), this.$.container);
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

  onNodeWillBeDeleted(nodeId: number) {
    const deletedNode = this.nodeStore_.getDomNode(nodeId) as ChildNode;
    if (deletedNode) {
      this.nodeStore_.removeDomNode(deletedNode);
      deletedNode.remove();
    }
    const root = this.nodeStore_.getDomNode(chrome.readingMode.rootId);
    if (this.hasContent_ && !root?.textContent) {
      this.hasContent_ = false;
      chrome.readingMode.onNoTextContent();
    }
  }

  languageChanged() {
    this.$.toolbar.updateFonts();
    if (this.isReadAloudEnabled_) {
      this.voiceLanguageController_.onPageLanguageChanged();
    }
    TextSegmenter.getInstance().updateLanguage(
        chrome.readingMode.baseLanguageForSpeech);
  }

  protected computeIsReadAloudPlayable(): boolean {
    return this.hasContent_ && this.speechEngineLoaded_ &&
        !!this.selectedVoice_ && !this.willDrawAgainSoon_;
  }

  protected onKeyDown_(e: KeyboardEvent) {
    if (e.key === 'k') {
      e.stopPropagation();
      if (this.speechController_.isSpeechActive()) {
        this.logger_.logSpeechStopSource(
            chrome.readingMode.keyboardShortcutStopSource);
      }
      this.onPlayPauseClick_();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'read-anything-app': AppElement;
  }
}

customElements.define(AppElement.is, AppElement);
