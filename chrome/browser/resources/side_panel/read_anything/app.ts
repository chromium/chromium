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
import {getCurrentSpeechRate, minOverflowLengthToScroll, playFromSelectionTimeout} from './common.js';
import type {LanguageToastElement} from './language_toast.js';
import {NodeStore} from './node_store.js';
import {ReadAloudHighlighter} from './read_aloud/highlighter.js';
import {SpeechController} from './read_aloud/speech_controller.js';
import type {SpeechListener} from './read_aloud/speech_controller.js';
import {PauseActionSource} from './read_aloud/speech_model.js';
import type {SpeechPlayingState} from './read_aloud/speech_model.js';
import {VoicePackController} from './read_aloud/voice_pack_controller.js';
import {WordBoundaries} from './read_aloud/word_boundaries.js';
import type {WordBoundaryState} from './read_aloud/word_boundaries.js';
import {ReadAnythingLogger, TimeFrom} from './read_anything_logger.js';
import type {ReadAnythingToolbarElement} from './read_anything_toolbar.js';
import type {SpeechBrowserProxy} from './speech_browser_proxy.js';
import {SpeechBrowserProxyImpl} from './speech_browser_proxy.js';
import {areVoicesEqual, AVAILABLE_GOOGLE_TTS_LOCALES, convertLangOrLocaleForVoicePackManager, convertLangOrLocaleToExactVoicePackLocale, convertLangToAnAvailableLangIfPresent, doesLanguageHaveNaturalVoices, getNaturalVoiceOrDefault, getVoicePackConvertedLangIfExists, isNatural, isVoicePackStatusError, isVoicePackStatusSuccess, mojoVoicePackStatusToVoicePackStatusEnum, VoiceClientSideStatusCode, VoicePackServerStatusErrorCode, VoicePackServerStatusSuccessCode} from './voice_language_util.js';
import type {VoicePackStatus} from './voice_language_util.js';
import {VoiceNotificationManager} from './voice_notification_manager.js';

const AppElementBase = WebUiListenerMixinLit(CrLitElement);

interface UtteranceSettings {
  lang: string;
  volume: number;
  pitch: number;
  rate: number;
}

const linkDataAttribute = 'link';

// The maximum speech length that should be used with remote voices
// due to a TTS engine bug with voices timing out on too-long text.
export const MAX_SPEECH_LENGTH: number = 175;

export interface AppElement {
  $: {
    toolbar: ReadAnythingToolbarElement,
    appFlexParent: HTMLElement,
    container: HTMLElement,
    containerParent: HTMLElement,
    languageToast: LanguageToastElement,
  };
}

export class AppElement extends AppElementBase implements SpeechListener {
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
      imagesEnabled: {type: Boolean, reflect: true},
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

  private scrollingOnSelection_ = false;
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

  // After the first utterance has been spoken, we should assume that the
  // speech engine has loaded, and we shouldn't adjust the play / pause
  // disabled state based on the message.onStart callback to avoid flickering.
  private firstUtteranceSpoken_ = false;

  // When a new TTS Engine extension is loaded into reading mode, we want to try
  // to install new natural voices from it. However, the new engine isn't ready
  // until it calls onvoiceschanged, so set this and wait for that call to
  // request the install.
  private waitingForNewEngine_ = false;

  protected accessor selectedVoice_: SpeechSynthesisVoice|undefined;
  // The set of languages currently enabled for use by Read Aloud. This
  // includes user-enabled languages and auto-downloaded languages. The former
  // are stored in preferences. The latter are not.
  protected accessor enabledLangs_: string[] = [];

  // All possible available voices for the current speech engine.
  protected accessor availableVoices_: SpeechSynthesisVoice[] = [];
  // If a preview is playing, this is set to the voice the preview is playing.
  // Otherwise, this is undefined.
  protected accessor previewVoicePlaying_: SpeechSynthesisVoice|undefined;

  protected accessor localeToDisplayName_: {[locale: string]: string} = {};

  // Metrics captured for logging.
  private playSessionStartTime: number = -1;

  private notificationManager_ = VoiceNotificationManager.getInstance();
  private logger_: ReadAnythingLogger = ReadAnythingLogger.getInstance();
  private styleUpdater_: AppStyleUpdater;
  private speech_: SpeechBrowserProxy = SpeechBrowserProxyImpl.getInstance();
  private highlighter_: ReadAloudHighlighter =
      ReadAloudHighlighter.getInstance();
  private wordBoundaries_: WordBoundaries = WordBoundaries.getInstance();
  private nodeStore_: NodeStore = NodeStore.getInstance();
  private voicePackController_: VoicePackController =
      VoicePackController.getInstance();
  private speechController_: SpeechController = SpeechController.getInstance();
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

  private accessor imagesEnabled: boolean = false;

  // If the node id of the first text node that should be used by Read Aloud
  // has been set. This is null if the id has not been set.
  firstTextNodeSetForReadAloud: number|null = null;

  speechSynthesisLanguage: string;

  // With minor page changes, we redistill or redraw sometimes and end up losing
  // our reading position if read aloud has started. This keeps track of the
  // last position so we can check if it's still in the new page.
  private lastReadingId_: number|null = null;
  private lastReadingOffset_: number|null = null;

  constructor() {
    super();
    this.constructorTime = Date.now();
    this.logger_.logTimeFrom(
        TimeFrom.APP, this.startTime, this.constructorTime);
    this.isReadAloudEnabled_ = chrome.readingMode.isReadAloudEnabled;
    this.speechSynthesisLanguage = chrome.readingMode.baseLanguageForSpeech;
    this.styleUpdater_ = new AppStyleUpdater(this);
    this.nodeStore_.clear();
    ColorChangeUpdater.forDocument().start();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    // Even though disconnectedCallback isn't always called reliably in prod,
    // it is called in tests, and the speech extension timeout can cause
    // flakiness.
    this.voicePackController_.stopWaitingForSpeechExtension();
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

    this.showLoading();

    if (this.isReadAloudEnabled_) {
      this.speechController_.addListener(this);
      this.notificationManager_.addListener(this.$.languageToast);

      // Clear state. We don't do this in disconnectedCallback because that's
      // not always reliabled called.
      this.speech_.cancel();
      this.hasContent_ = false;
      this.firstUtteranceSpoken_ = false;
      this.firstTextNodeSetForReadAloud = null;
      this.nodeStore_.clearDomNodes();
      this.clearReadAloudState();

      this.speech_.setOnVoicesChanged(this.onVoicesChanged.bind(this));
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
      const selection: Selection = this.getSelection();
      assert(selection, 'no selection');
      if (!selection.anchorNode || !selection.focusNode) {
        // The selection was collapsed by clicking inside the selection.
        chrome.readingMode.onCollapseSelection();
        return;
      }

      const {anchorNodeId, anchorOffset, focusNodeId, focusOffset} =
          this.getSelectedIds();
      if (!anchorNodeId || !focusNodeId) {
        return;
      }

      // Only send this selection to the main panel if it is different than the
      // current main panel selection.
      const mainPanelAnchor =
          this.nodeStore_.getDomNode(chrome.readingMode.startNodeId);
      const mainPanelFocus =
          this.nodeStore_.getDomNode(chrome.readingMode.endNodeId);
      if (!mainPanelAnchor || !mainPanelAnchor.contains(selection.anchorNode) ||
          !mainPanelFocus || !mainPanelFocus.contains(selection.focusNode) ||
          selection.anchorOffset !== chrome.readingMode.startOffset ||
          selection.focusOffset !== chrome.readingMode.endOffset) {
        chrome.readingMode.onSelectionChange(
            anchorNodeId, anchorOffset, focusNodeId, focusOffset);
      }

      // If there's been a selection, clear the current Read Aloud highlight.
      if (anchorNodeId && focusNodeId) {
        // If speech is resumed, this won't be restored.
        // TODO: crbug.com/40927698 - Restore the previous highlight after
        // speech is resumed after a selection.
        this.highlighter_.clearHighlightFormatting();
      }
    };

    this.$.containerParent.onscroll = () => {
      chrome.readingMode.onScroll(this.scrollingOnSelection_);
      this.scrollingOnSelection_ = false;

      // If the reading mode panel was scrolled while read aloud is speaking,
      // we should disable autoscroll if the highlights are no longer visible,
      // and we should re-enable autoscroll if the highlights are now
      // visible.
      if (this.speechController_.isSpeechActive()) {
        this.highlighter_.updateAutoScroll();
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
      this.onImageDownloaded(nodeId);
    };

    chrome.readingMode.updateSelection = () => {
      this.updateSelection();
    };

    chrome.readingMode.updateVoicePackStatus =
        (lang: string, status: string) => {
          this.updateVoicePackStatus(lang, status);
        };

    chrome.readingMode.showLoading = () => {
      this.showLoading();
    };

    chrome.readingMode.showEmpty = () => {
      this.showEmpty();
    };

    chrome.readingMode.restoreSettingsFromPrefs = () => {
      this.restoreSettingsFromPrefs();
    };

    chrome.readingMode.languageChanged = () => {
      this.languageChanged();
    };

    chrome.readingMode.onLockScreen = () => {
      this.onLockScreen();
    };

    chrome.readingMode.onTtsEngineInstalled = () => {
      this.onTtsEngineInstalled();
    };

    chrome.readingMode.onNodeWillBeDeleted = (nodeId: number) => {
      this.onNodeWillBeDeleted(nodeId);
    };
  }

  private buildSubtree_(nodeId: number): Node {
    let htmlTag = chrome.readingMode.getHtmlTag(nodeId);
    const dataAttributes = new Map<string, string>();

    // Text nodes do not have an html tag.
    if (!htmlTag.length) {
      return this.createTextNode_(nodeId);
    }

    // For Google Docs, we extract text from Annotated Canvas. The Annotated
    // Canvas elements with text are leaf nodes with <rect> html tag.
    if (chrome.readingMode.isGoogleDocs &&
        chrome.readingMode.isLeafNode(nodeId)) {
      return this.createTextNode_(nodeId);
    }

    // getHtmlTag might return '#document' which is not a valid to pass to
    // createElement.
    if (htmlTag === '#document') {
      htmlTag = 'div';
    }

    // Only one body tag is allowed per document.
    if (htmlTag === 'body') {
      htmlTag = 'div';
    }

    // details tags hide content beneath them if closed. If opened, there is
    // content underneath we should show, but surrounding it with a generic
    // details tag causes it to be hidden in reading mode. So use a div instead.
    // In the cases that the details are closed, then nothing will be returned
    // beneath the details tag so nothing is rendered on reading mode.
    if (htmlTag === 'details') {
      htmlTag = 'div';
    }

    // Images will be written to a canvas.
    if (htmlTag === 'img') {
      htmlTag = 'canvas';
    }

    const url = chrome.readingMode.getUrl(nodeId);

    if (!this.shouldShowLinks() && htmlTag === 'a') {
      htmlTag = 'span';
      dataAttributes.set(linkDataAttribute, url ?? '');
    }

    const element = document.createElement(htmlTag);
    // Add required data attributes.
    for (const [attr, val] of dataAttributes) {
      element.dataset[attr] = val;
    }
    this.nodeStore_.setDomNode(element, nodeId);
    const direction = chrome.readingMode.getTextDirection(nodeId);
    if (direction) {
      element.setAttribute('dir', direction);
    }

    if (element.nodeName === 'CANVAS') {
      this.nodeStore_.addImageToFetch(nodeId);
      const altText = chrome.readingMode.getAltText(nodeId);
      element.setAttribute('alt', altText);
      element.style.display = chrome.readingMode.imagesEnabled ? '' : 'none';
      element.classList.add('downloaded-image');
    }

    if (url && element.nodeName === 'A') {
      element.setAttribute('href', url);
      element.onclick = () => {
        chrome.readingMode.onLinkClicked(nodeId);
      };
    }
    const language = chrome.readingMode.getLanguage(nodeId);
    if (language) {
      element.setAttribute('lang', language);
    }

    this.appendChildSubtrees_(element, nodeId);
    return element;
  }

  // TODO: crbug.com/40910704- Potentially hide links during distillation.
  private shouldShowLinks(): boolean {
    // Links should only show when Read Aloud is paused.
    return chrome.readingMode.linksEnabled &&
        !this.speechController_.isSpeechActive();
  }

  private appendChildSubtrees_(node: Node, nodeId: number) {
    for (const childNodeId of chrome.readingMode.getChildren(nodeId)) {
      const childNode = this.buildSubtree_(childNodeId);
      node.appendChild(childNode);
    }
  }

  private createTextNode_(nodeId: number): Node {
    // When creating text nodes, save the first text node id. We need this
    // node id to call InitAXPosition in playSpeech. If it's not saved here,
    // we have to retrieve it through a DOM search such as createTreeWalker,
    // which can be computationally expensive.
    if (!this.firstTextNodeSetForReadAloud) {
      this.firstTextNodeSetForReadAloud = nodeId;
      this.speechController_.initializeSpeechTree(nodeId);
    }

    const textContent = chrome.readingMode.getTextContent(nodeId);
    const textNode = document.createTextNode(textContent);
    this.nodeStore_.setDomNode(textNode, nodeId);
    const isOverline = chrome.readingMode.isOverline(nodeId);
    const shouldBold = chrome.readingMode.shouldBold(nodeId);

    if (!shouldBold && !isOverline) {
      return textNode;
    }

    const htmlTag = shouldBold ? 'b' : 'span';
    const parentElement = document.createElement(htmlTag);
    if (isOverline) {
      parentElement.style.textDecoration = 'overline';
    }
    parentElement.appendChild(textNode);
    return parentElement;
  }

  showEmpty() {
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

  showLoading() {
    this.emptyStateImagePath_ = '//resources/images/throbber_small.svg';
    this.emptyStateDarkImagePath_ =
        '//resources/images/throbber_small_dark.svg';
    this.emptyStateHeading_ =
        loadTimeData.getString('readAnythingLoadingMessage');
    this.emptyStateSubheading_ = '';
    this.hasContent_ = false;
    if (this.isReadAloudEnabled_) {
      this.speech_.cancel();
      this.clearReadAloudState();
    }
  }

  // TODO: crbug.com/40927698 - Handle focus changes for speech, including
  // updating speech state.
  updateContent() {
    // Each time we rebuild the subtree, we should clear the node id of the
    // first text node.
    this.firstTextNodeSetForReadAloud = null;

    // This shouldn't happen. If it does, there is likely a bug, so log it so
    // we can monitor it.
    if (this.speechController_.isSpeechActive()) {
      console.error(
          'updateContent called while speech is active. ',
          'There may be a bug.');
      this.logger_.logSpeechStopSource(
          chrome.readingMode.unexpectedUpdateContentStopSource);
    }
    const previousSpeechPlayingState = {...this.speechController_.getState()};
    const previousWordBoundaryState = {...this.wordBoundaries_.state};

    this.speech_.cancel();
    this.clearReadAloudState();
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
    const node = this.buildSubtree_(rootId);
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
      }
      return;
    }

    if (this.previousRootId_ !== rootId) {
      this.previousRootId_ = rootId;
      this.logger_.logNewPage(/*speechPlayed=*/ false);
    }

    // Always load images even if they are disabled to ensure a fast response
    // when toggling.
    this.loadImages_();

    this.isDocsLoadMoreButtonVisible_ =
        chrome.readingMode.isDocsLoadMoreButtonVisible;

    container.scrollTop = 0;
    this.hasContent_ = true;
    container.appendChild(node);
    this.updateImages_();

    // If the previous reading position still exists and we haven't reached the
    // end of speech, keep that spot.
    if (previousSpeechPlayingState.hasSpeechBeenTriggered) {
      this.setPreviousReadingPositionIfExists_(
          previousWordBoundaryState, previousSpeechPlayingState);
    }
  }

  private setPreviousReadingPositionIfExists_(
      previousWordBoundaryState: WordBoundaryState,
      previousSpeechPlayingState: SpeechPlayingState) {
    if (this.lastReadingId_ === null || this.lastReadingOffset_ === null) {
      return;
    }

    if (this.nodeStore_.getDomNode(this.lastReadingId_)) {
      this.movePlaybackToNode_(this.lastReadingId_, this.lastReadingOffset_);
      this.speechController_.setState(previousSpeechPlayingState);
      this.wordBoundaries_.state = {...previousWordBoundaryState};
      // Since we're setting the reading position after a content update when
      // we're paused, redraw the highlight after moving the traversal state to
      // the right spot above.
      this.highlightCurrentGranularity(chrome.readingMode.getCurrentText());
    } else {
      this.lastReadingId_ = null;
      this.lastReadingOffset_ = null;
    }
  }

  async onImageDownloaded(nodeId: number) {
    const data = chrome.readingMode.getImageBitmap(nodeId);
    const element = this.nodeStore_.getDomNode(nodeId);
    if (data && element && element instanceof HTMLCanvasElement) {
      element.width = data.width;
      element.height = data.height;
      element.style.zoom = data.scale.toString();
      const context = element.getContext('2d');
      // Context should not be null unless another was already requested.
      assert(context);
      const imgData = new ImageData(data.data, data.width);
      const bitmap = await createImageBitmap(imgData, {
        colorSpaceConversion: 'none',
        premultiplyAlpha: 'premultiply',
      });
      context.drawImage(bitmap, 0, 0);
    }
  }

  private loadImages_() {
    if (!chrome.readingMode.imagesFeatureEnabled) {
      return;
    }

    this.nodeStore_.fetchImages();
  }

  getSelection(): any {
    assert(this.shadowRoot, 'no shadow root');
    return this.shadowRoot.getSelection();
  }

  updateSelection() {
    const selection: Selection = this.getSelection();
    selection.removeAllRanges();

    const range = new Range();
    const startNodeId = chrome.readingMode.startNodeId;
    const endNodeId = chrome.readingMode.endNodeId;
    let startOffset = chrome.readingMode.startOffset;
    let endOffset = chrome.readingMode.endOffset;
    let startNode = this.nodeStore_.getDomNode(startNodeId);
    let endNode = this.nodeStore_.getDomNode(endNodeId);
    if (!startNode || !endNode) {
      return;
    }

    // Range.setStart/setEnd behaves differently if the node is an element or a
    // text node. If the former, the offset refers to the index of the children.
    // If the latter, the offset refers to the character offset inside the text
    // node. The start and end nodes are elements if they've been read aloud
    // because we add formatting to the text that wasn't there before. However,
    // the information we receive from chrome.readingMode is always the id of a
    // text node and character offset for that text, so find the corresponding
    // text child here and adjust the offset
    if (startNode.nodeType !== Node.TEXT_NODE) {
      const startTreeWalker =
          document.createTreeWalker(startNode, NodeFilter.SHOW_TEXT);
      while (startTreeWalker.nextNode()) {
        const textNodeLength = startTreeWalker.currentNode.textContent!.length;
        // Once we find the child text node inside which the starting index
        // fits, update the start node to be that child node and the adjusted
        // offset will be relative to this child node
        if (startOffset < textNodeLength) {
          startNode = startTreeWalker.currentNode;
          break;
        }

        startOffset -= textNodeLength;
      }
    }
    if (endNode.nodeType !== Node.TEXT_NODE) {
      const endTreeWalker =
          document.createTreeWalker(endNode, NodeFilter.SHOW_TEXT);
      while (endTreeWalker.nextNode()) {
        const textNodeLength = endTreeWalker.currentNode.textContent!.length;
        if (endOffset <= textNodeLength) {
          endNode = endTreeWalker.currentNode;
          break;
        }

        endOffset -= textNodeLength;
      }
    }

    // Gmail will try to select text when collapsing the node. At the same time,
    // the node contents are then shortened because of the collapse which causes
    // the range to go out of bounds. When this happens we should reset the
    // selection.
    try {
      range.setStart(startNode, startOffset);
      range.setEnd(endNode, endOffset);
    } catch (err) {
      selection.removeAllRanges();
      return;
    }

    selection.addRange(range);

    // Scroll the start node into view. ScrollIntoView is available on the
    // Element class.
    const startElement = startNode.nodeType === Node.ELEMENT_NODE ?
        startNode as Element :
        startNode.parentElement;
    if (!startElement) {
      return;
    }
    this.scrollingOnSelection_ = true;
    startElement.scrollIntoViewIfNeeded();
  }

  protected updateLinks_(shouldRehighlightCurrentNodes: boolean = true) {
    if (!this.shadowRoot) {
      return;
    }

    const originallyHadHighlights = this.highlighter_.hasCurrentHighlights();
    const selector = this.shouldShowLinks() ? 'span[data-link]' : 'a';
    const elements = this.shadowRoot.querySelectorAll(selector);

    for (const elem of elements) {
      assert(elem instanceof HTMLElement, 'link is not an HTMLElement');
      const nodeId = this.nodeStore_.getAxId(elem);
      assert(nodeId !== undefined, 'link node id is undefined');
      const replacement = this.buildSubtree_(nodeId);
      this.nodeStore_.replaceDomNode(elem, replacement);
    }

    // Rehighlight the current granularity text after links have been
    // toggled on or off to ensure the entire granularity segment is
    // highlighted.
    if (shouldRehighlightCurrentNodes && originallyHadHighlights) {
      this.highlightCurrentGranularity(chrome.readingMode.getCurrentText());
    }
    this.loadImages_();
  }

  protected updateImages_() {
    if (!this.shadowRoot) {
      return;
    }

    this.imagesEnabled = chrome.readingMode.imagesEnabled;
    if (this.imagesEnabled) {
      this.nodeStore_.clearHiddenImageNodes();
    }
    // There is some strange issue where the HTML css application does not work
    // on canvases.
    for (const canvas of this.shadowRoot.querySelectorAll('canvas')) {
      canvas.style.display = this.imagesEnabled ? '' : 'none';
      this.markTextNodesHiddenIfImagesHidden_(canvas);
    }
    for (const canvas of this.shadowRoot.querySelectorAll('figure')) {
      canvas.style.display = this.imagesEnabled ? '' : 'none';
      this.markTextNodesHiddenIfImagesHidden_(canvas);
    }
  }

  private async markTextNodesHiddenIfImagesHidden_(node: Node) {
    if (this.imagesEnabled) {
      return;
    }

    // Do this asynchronously so we don't block the UI on large pages.
    await new Promise(() => {
      setTimeout(() => {
        const id = this.nodeStore_.getAxId(node);
        if (node.nodeType === Node.TEXT_NODE) {
          if (id) {
            this.nodeStore_.hideImageNode(id);
          }
          return;
        }

        // Since read aloud looks at the text nodes, we want to store those ids
        // so we don't read out text that is not visible.
        const startTreeWalker =
            document.createTreeWalker(node, NodeFilter.SHOW_ALL);
        while (startTreeWalker.nextNode()) {
          const id = this.nodeStore_.getAxId(startTreeWalker.currentNode);
          if (id) {
            this.nodeStore_.hideImageNode(id);
          }
        }
      });
    });
  }

  protected onDocsLoadMoreButtonClick_() {
    chrome.readingMode.onScrolledToBottom();
  }

  updateVoicePackStatus(lang: string, status: string) {
    this.voicePackController_.stopWaitingForSpeechExtension();

    if (!lang) {
      return;
    }

    const newVoicePackStatus = mojoVoicePackStatusToVoicePackStatusEnum(status);

    // Keep the server responses
    this.voicePackController_.setServerStatus(lang, newVoicePackStatus);

    // Update application state
    this.updateApplicationState(lang, newVoicePackStatus);

    if (isVoicePackStatusError(newVoicePackStatus) &&
        this.voicePackController_.disableLangIfNoVoices(lang)) {
      this.enabledLangs_ = this.voicePackController_.getEnabledLangs();
    }
  }

  // Store client side voice pack state and trigger side effects
  private updateApplicationState(
      lang: string, newVoicePackStatus: VoicePackStatus) {
    if (isVoicePackStatusSuccess(newVoicePackStatus)) {
      const newStatusCode = newVoicePackStatus.code;

      switch (newStatusCode) {
        case VoicePackServerStatusSuccessCode.NOT_INSTALLED:
          this.voicePackController_.triggerInstall(lang);
          break;
        case VoicePackServerStatusSuccessCode.INSTALLING:
          // Do nothing- we mark our local state as installing when we send the
          // request. Locally, we may time out a slow request and mark it as
          // errored, and we don't want to overwrite that state here.
          break;
        case VoicePackServerStatusSuccessCode.INSTALLED:
          // Force a refresh of the voices list since we might not get an update
          // the voices have changed.
          this.getVoices_(/*forceRefresh=*/ true);
          this.autoSwitchVoice_(lang);

          // Some languages may require a download from the voice pack
          // but may not have associated natural voices.
          const languageHasNaturalVoices = doesLanguageHaveNaturalVoices(lang);

          // Even though the voice may be installed on disk, it still may not be
          // available to the speechSynthesis API. Check whether to mark the
          // voice as AVAILABLE or INSTALLED_AND_UNAVAILABLE
          const voicesForLanguageAreAvailable =
              this.voicePackController_.getAvailableVoices().some(
                  voice =>
                      ((isNatural(voice) || !languageHasNaturalVoices) &&
                       getVoicePackConvertedLangIfExists(voice.lang) === lang));

          // If natural voices are currently available for the language or the
          // language does not support natural voices, set the status to
          // available. Otherwise, set the status to install and unavailabled.
          this.voicePackController_.setLocalStatus(
              lang,
              voicesForLanguageAreAvailable ?
                  VoiceClientSideStatusCode.AVAILABLE :
                  VoiceClientSideStatusCode.INSTALLED_AND_UNAVAILABLE);
          break;
        default:
          // This ensures the switch statement is exhaustive
          return newStatusCode satisfies never;
      }
    } else if (isVoicePackStatusError(newVoicePackStatus)) {
      this.autoSwitchVoice_(lang);
      const newStatusCode = newVoicePackStatus.code;

      switch (newStatusCode) {
        case VoicePackServerStatusErrorCode.OTHER:
        case VoicePackServerStatusErrorCode.WRONG_ID:
        case VoicePackServerStatusErrorCode.NEED_REBOOT:
        case VoicePackServerStatusErrorCode.UNSUPPORTED_PLATFORM:
          this.voicePackController_.setLocalStatus(
              lang, VoiceClientSideStatusCode.ERROR_INSTALLING);
          break;
        case VoicePackServerStatusErrorCode.ALLOCATION:
          this.voicePackController_.setLocalStatus(
              lang, VoiceClientSideStatusCode.INSTALL_ERROR_ALLOCATION);
          break;
        default:
          // This ensures the switch statement is exhaustive
          return newStatusCode satisfies never;
      }
    } else {
      // Couldn't parse the response
      this.voicePackController_.setLocalStatus(
          lang, VoiceClientSideStatusCode.ERROR_INSTALLING);
    }
  }

  protected onLanguageMenuOpen_() {
    this.notificationManager_.removeListener(this.$.languageToast);
  }

  protected onLanguageMenuClose_() {
    this.notificationManager_.addListener(this.$.languageToast);
  }

  onVoicesChanged() {
    if (this.waitingForNewEngine_) {
      this.enabledLangs_.forEach(lang => {
        this.installVoicePackIfPossible(
            lang,
            /* onlyInstallExactGoogleLocaleMatch=*/ true,
            /* retryIfPreviousInstallFailed= */ true);
      });
      this.waitingForNewEngine_ = false;
      return;
    }

    const hadAvailableVoices = this.voicePackController_.hasAvailableVoices();
    // Get a new list of voices. This should be done before we call
    // refreshVoicePackStatuses();
    this.getVoices_(/*forceRefresh=*/ true);

    // TODO: crbug.com/390435037 - Simplify logic around loading voices and
    // language availability, especially around the new TTS engine.

    // <if expr="not is_chromeos">
    if (this.voicePackController_.enableNowAvailableLangs()) {
      this.enabledLangs_ = this.voicePackController_.getEnabledLangs();
    }
    // </if>

    if (!hadAvailableVoices && this.voicePackController_.hasAvailableVoices()) {
      // If we go from having no available voices to having voices available,
      // restore voice settings from preferences.
      this.restoreEnabledLanguagesFromPref();
      this.selectPreferredVoice();
    }

    // If voice was selected automatically and not by the user, check if
    // there's a higher quality voice available now.
    if (!this.currentVoiceIsUserChosen_()) {
      const naturalVoicesForLang =
          this.voicePackController_.getAvailableVoices().filter(
              voice => isNatural(voice) &&
                  voice.lang.startsWith(
                      chrome.readingMode.baseLanguageForSpeech));

      if (naturalVoicesForLang) {
        this.selectedVoice_ = naturalVoicesForLang[0];
        this.resetSpeechPostSettingChange_();
      }
    }

    // Now that the voice list has changed, refresh the VoicePackStatuses in
    // case a language has been uninstalled.
    this.voicePackController_.refreshVoicePackStatuses();

    // If the selected voice is now unavailable, such as after an uninstall,
    // reselect a new voice.
    if (this.selectedVoice_ &&
        !this.voicePackController_.isVoiceAvailable(this.selectedVoice_)) {
      this.selectedVoice_ = undefined;
    }

    if (!this.selectedVoice_) {
      this.getSpeechSynthesisVoice();
    }
  }

  getSpeechSynthesisVoice(): SpeechSynthesisVoice|undefined {
    if (!this.selectedVoice_) {
      this.selectedVoice_ = this.defaultVoice();
    }
    return this.selectedVoice_;
  }

  defaultVoice(): SpeechSynthesisVoice|undefined {
    const baseLang = this.speechSynthesisLanguage;
    const allPossibleVoices = this.getVoices_();
    const voicesForLanguage =
        allPossibleVoices.filter(voice => voice.lang.startsWith(baseLang));

    if (!voicesForLanguage || (voicesForLanguage.length === 0)) {
      // Stay with the current voice if no voices are available for this
      // language.
      return this.selectedVoice_ ? this.selectedVoice_ :
                                   getNaturalVoiceOrDefault(allPossibleVoices);
    }

    // First try to choose a voice only from currently enabled locales for this
    // language.
    const voicesForCurrentEnabledLocale = voicesForLanguage.filter(
        v => this.voicePackController_.isLangEnabled(v.lang));
    if (!voicesForCurrentEnabledLocale ||
        !voicesForCurrentEnabledLocale.length) {
      // If there's no enabled locales for this language, check for any other
      // voices for enabled locales.
      const allVoicesForEnabledLocales = allPossibleVoices.filter(
          v => this.voicePackController_.isLangEnabled(v.lang));
      if (!allVoicesForEnabledLocales.length) {
        // If there are no voices for the enabled locales, or no enabled
        // locales at all, we can't select a voice. So return undefined so we
        // can disable the play button.
        return undefined;
      } else {
        return getNaturalVoiceOrDefault(allVoicesForEnabledLocales);
      }
    }

    return getNaturalVoiceOrDefault(voicesForCurrentEnabledLocale);
  }

  // Attempt to get a new voice using the current language. In theory, the
  // previously unavailable voice should no longer be showing up in
  // getVoices, but we ensure that the alternative voice does not match
  // the previously unavailable voice as an extra measure. This method should
  // only be called when speech synthesis returns an error.
  getAlternativeVoice(unavailableVoice: SpeechSynthesisVoice|undefined):
      SpeechSynthesisVoice|undefined {
    const newVoice = this.defaultVoice();

    // If the default voice is not the same as the original, unavailable voice,
    // use that, only if the new voice is also defined.
    if (newVoice !== undefined && !areVoicesEqual(newVoice, unavailableVoice)) {
      return newVoice;
    }

    // If the default voice won't work, try another voice in that language.
    const baseLang = this.speechSynthesisLanguage;
    const voicesForLanguage =
        this.getVoices_().filter(voice => voice.lang.startsWith(baseLang));

    // TODO: crbug.com/40927698 - It's possible we can get stuck in an infinite
    // loop of jumping back and forth between two or more invalid voices, if
    // multiple voices are invalid. Investigate if we need to do more to handle
    // this case.

    // TODO: crbug.com/336596926 - If there still aren't voices for the
    // language, attempt to fallback to the browser language, if we're using
    // the page language.
    if (!voicesForLanguage || (voicesForLanguage.length === 0)) {
      return undefined;
    }

    let voiceIndex = 0;
    while (voiceIndex < voicesForLanguage.length) {
      if (!areVoicesEqual(voicesForLanguage[voiceIndex], unavailableVoice)) {
        // Return another voice in the same language, ensuring we're not
        // returning the previously unavailable voice for extra safety.
        return voicesForLanguage[voiceIndex];
      }
      voiceIndex++;
    }

    // TODO: crbug.com/336596926 - Handle language updates if there aren't any
    // available voices in the current language other than the unavailable
    // voice.
    return undefined;
  }

  private getVoices_(forceRefresh: boolean = false): SpeechSynthesisVoice[] {
    if (this.voicePackController_.refreshAvailableVoices(forceRefresh)) {
      this.availableVoices_ = this.voicePackController_.getAvailableVoices();
      this.localeToDisplayName_ =
          this.voicePackController_.getDisplayNamesForLocaleCodes();
    }
    return this.availableVoices_;
  }

  protected onPreviewVoice_(
      event: CustomEvent<{previewVoice: SpeechSynthesisVoice}>) {
    event.preventDefault();
    event.stopPropagation();

    this.speechController_.stopSpeech(PauseActionSource.VOICE_PREVIEW);

    // If there's no previewVoice, return after stopping the current preview
    if (!event.detail) {
      this.previewVoicePlaying_ = undefined;
      return;
    }

    const defaultUtteranceSettings = this.defaultUtteranceSettings();
    const utterance = new SpeechSynthesisUtterance(
        loadTimeData.getString('readingModeVoicePreviewText'));
    const voice = event.detail.previewVoice;
    utterance.voice = voice;
    utterance.lang = defaultUtteranceSettings.lang;
    utterance.volume = defaultUtteranceSettings.volume;
    utterance.pitch = defaultUtteranceSettings.pitch;
    utterance.rate = defaultUtteranceSettings.rate;

    utterance.onstart = event => {
      this.previewVoicePlaying_ = event.utterance.voice || undefined;
    };

    utterance.onend = () => {
      this.previewVoicePlaying_ = undefined;
    };

    // TODO: crbug.com/40927698 - There should probably be more sophisticated
    // error handling for voice previews, but for now, simply setting the
    // preview voice to null should be sufficient to reset state if an error is
    // encountered during a preview.
    utterance.onerror = () => {
      this.previewVoicePlaying_ = undefined;
    };

    this.speech_.speak(utterance);
  }

  protected onVoiceMenuClose_(
      event: CustomEvent<{voicePlayingWhenMenuOpened: boolean}>) {
    event.preventDefault();
    event.stopPropagation();

    // TODO: crbug.com/323912186 - Handle when menu is closed mid-preview and
    // the user presses play/pause button.
    if (!this.speechController_.isSpeechActive() &&
        event.detail.voicePlayingWhenMenuOpened) {
      this.playSpeech();
    }
  }

  protected onPlayPauseClick_() {
    if (this.speechController_.isSpeechActive()) {
      this.logSpeechPlaySession_();
      this.speechController_.stopSpeech(PauseActionSource.BUTTON_CLICK);
    } else {
      this.playSessionStartTime = Date.now();
      this.playSpeech();
    }
  }

  onIsSpeechActiveChange(): void {
    this.isSpeechActive_ = this.speechController_.isSpeechActive();
  }

  onIsAudioCurrentlyPlayingChange(): void {
    this.isAudioCurrentlyPlaying_ =
        this.speechController_.isAudioCurrentlyPlaying();
  }

  onPause() {
    // Restore links if they're enabled when speech pauses. Don't restore links
    // if it's paused from a non-pause button (e.g. voice previews) so the links
    // don't flash off and on.
    if (chrome.readingMode.linksEnabled &&
        this.speechController_.isPausedFromButton()) {
      this.updateLinks_();
    }
  }

  private logSpeechPlaySession_() {
    // Don't log a playback session just in case something has gotten out of
    // sync and we call stopSpeech before playSpeech.
    if (this.playSessionStartTime > 0) {
      this.logger_.logSpeechPlaySession(
          this.playSessionStartTime, this.selectedVoice_);
      this.playSessionStartTime = -1;
    }
  }

  protected playNextGranularity_() {
    this.speechController_.setIsSpeechBeingRepositioned(true);

    this.speech_.cancel();
    this.highlighter_.resetPreviousHighlight();
    // Reset the word boundary index whenever we move the granularity position.
    this.wordBoundaries_.resetToDefaultState();
    chrome.readingMode.movePositionToNextGranularity();

    if (!this.highlightAndPlayMessage()) {
      this.onSpeechFinished();
    }
  }

  protected playPreviousGranularity_() {
    this.speechController_.setIsSpeechBeingRepositioned(true);
    this.speech_.cancel();
    // This must be called BEFORE calling
    // chrome.readingMode.movePositionToPreviousGranularity so we can accurately
    // determine what's currently being highlighted.
    this.highlighter_.removeCurrentHighlight();
    this.highlighter_.resetPreviousHighlight();
    // Reset the word boundary index whenever we move the granularity position.
    this.wordBoundaries_.resetToDefaultState();
    chrome.readingMode.movePositionToPreviousGranularity();

    if (!this.highlightAndPlayMessage(/*isInterrupted=*/ false,
                                      /*isMovingBackward=*/ true)) {
      this.onSpeechFinished();
    }
  }

  playSpeech() {
    const container = this.$.container;
    const {anchorNode, anchorOffset, focusNode, focusOffset} =
        this.getSelection();
    const hasSelection =
        anchorNode !== focusNode || anchorOffset !== focusOffset;
    if (this.speechController_.hasSpeechBeenTriggered() &&
        !this.speechController_.isSpeechActive()) {
      const pausedFromButton = this.speechController_.isPausedFromButton();

      let playedFromSelection = false;
      if (hasSelection) {
        this.speech_.cancel();
        this.wordBoundaries_.resetToDefaultState();
        playedFromSelection = this.playFromSelection();
      }

      if (!playedFromSelection) {
        if (pausedFromButton && !this.wordBoundaries_.hasBoundaries()) {
          // If word boundaries aren't supported for the given voice, we should
          // still continue to use synth.resume, as this is preferable to
          // restarting the current message.
          this.speech_.resume();
        } else {
          this.speech_.cancel();
          if (!this.highlightAndPlayInterruptedMessage()) {
            // Ensure we're updating Read Aloud state if there's no text to
            // speak.
            this.onSpeechFinished();
          }
        }
      }

      this.speechController_.setIsSpeechActive(true);
      this.speechController_.setIsSpeechBeingRepositioned(false);

      // Hide links when speech resumes. We only hide links when the page was
      // paused from the play/pause button.
      if (chrome.readingMode.linksEnabled && pausedFromButton) {
        // Toggle links and ensure that the new nodes are also highlighted.
        this.updateLinks_(
            /* shouldRehiglightCurrentNodes= */ !playedFromSelection);
      }

      // If the current read highlight has been cleared from a call to
      // updateContent, such as via a preference change, rehighlight the nodes
      // after a pause.
      if (!playedFromSelection) {
        this.highlightCurrentGranularity(chrome.readingMode.getCurrentText());
      }

      return;
    }
    if (container.textContent) {
      // Log that we're playing speech on a new page, but not when resuming.
      // This helps us compare how many reading mode pages are opened with
      // speech played and without speech played. Counting resumes would
      // inflate the speech played number.
      this.logger_.logNewPage(/*speechPlayed=*/ true);
      this.speechController_.setIsSpeechActive(true);
      this.speechController_.setHasSpeechBeenTriggered(true);
      this.speechController_.setIsSpeechBeingRepositioned(false);

      // Hide links when speech begins playing.
      if (chrome.readingMode.linksEnabled) {
        this.updateLinks_();
      }

      const playedFromSelection = hasSelection && this.playFromSelection();
      if (!playedFromSelection && this.firstTextNodeSetForReadAloud) {
        this.speechController_.initializeSpeechTree(
            this.firstTextNodeSetForReadAloud);
        if (!this.highlightAndPlayMessage()) {
          // Ensure we're updating Read Aloud state if there's no text to speak.
          this.onSpeechFinished();
        }
      }
    }
  }

  private getSelectedIds(): {
    anchorNodeId: number|undefined,
    anchorOffset: number,
    focusNodeId: number|undefined,
    focusOffset: number,
  } {
    const {anchorNode, anchorOffset, focusNode, focusOffset} =
        this.getSelection();
    let anchorNodeId = this.nodeStore_.getAxId(anchorNode);
    let focusNodeId = this.nodeStore_.getAxId(focusNode);
    let adjustedAnchorOffset = anchorOffset;
    let adjustedFocusOffset = focusOffset;
    if (!anchorNodeId) {
      anchorNodeId = this.highlighter_.getAncestorId(anchorNode);
      adjustedAnchorOffset += this.highlighter_.getOffsetInAncestor(anchorNode);
    }
    if (!focusNodeId) {
      focusNodeId = this.highlighter_.getAncestorId(focusNode);
      adjustedFocusOffset += this.highlighter_.getOffsetInAncestor(focusNode);
    }
    return {
      anchorNodeId: anchorNodeId,
      anchorOffset: adjustedAnchorOffset,
      focusNodeId: focusNodeId,
      focusOffset: adjustedFocusOffset,
    };
  }

  playFromSelection(): boolean {
    const selection = this.getSelection();
    if (!this.firstTextNodeSetForReadAloud || !selection) {
      return false;
    }

    const anchorNodeId = chrome.readingMode.startNodeId;
    const anchorOffset = chrome.readingMode.startOffset;
    const focusNodeId = chrome.readingMode.endNodeId;
    const focusOffset = chrome.readingMode.endOffset;

    // If only one of the ids is present, use that one.
    let startingNodeId: number|undefined =
        anchorNodeId ? anchorNodeId : focusNodeId;
    let startingOffset = anchorNodeId ? anchorOffset : focusOffset;
    // If both are present, start with the node that is sooner in the page.
    if (anchorNodeId && focusNodeId) {
      if (anchorNodeId === focusNodeId) {
        startingOffset = Math.min(anchorOffset, focusOffset);
      } else {
        const pos =
            selection.anchorNode.compareDocumentPosition(selection.focusNode);
        const focusIsFirst = pos === Node.DOCUMENT_POSITION_PRECEDING;
        startingNodeId = focusIsFirst ? focusNodeId : anchorNodeId;
        startingOffset = focusIsFirst ? focusOffset : anchorOffset;
      }
    }

    if (!startingNodeId) {
      return false;
    }

    // Clear the selection so we don't keep trying to play from the same
    // selection every time they press play.
    selection.removeAllRanges();

    // Iterate through the page from the beginning until we get to the
    // selection. This is so clicking previous works before the selection and
    // so the previous highlights are properly set.
    chrome.readingMode.resetGranularityIndex();
    // Iterate through the nodes asynchronously so that we can show the spinner
    // in the toolbar while we move up to the selection.
    setTimeout(() => {
      this.movePlaybackToNode_(startingNodeId, startingOffset);
      // Set everything to previous and then play the next granularity, which
      // includes the selection.
      this.highlighter_.resetPreviousHighlight();
      if (!this.highlightAndPlayMessage()) {
        this.onSpeechFinished();
      }
    }, playFromSelectionTimeout);

    return true;
  }

  private movePlaybackToNode_(nodeId: number, offset: number): void {
    let currentTextIds = chrome.readingMode.getCurrentText();
    let hasCurrentText = currentTextIds.length > 0;
    // Since a node could spread across multiple granularities, we use the
    // offset to determine if the selected text is in this granularity or if
    // we have to move to the next one.
    let startOfSelectionIsInCurrentText = currentTextIds.includes(nodeId) &&
        chrome.readingMode.getCurrentTextEndIndex(nodeId) > offset;
    while (hasCurrentText && !startOfSelectionIsInCurrentText) {
      this.highlightCurrentGranularity(
          currentTextIds, /*scrollIntoView=*/ false,
          /*shouldUpdateSentenceHighlight=*/ true,
          /*shouldSetLastReadingPos=*/ false);
      chrome.readingMode.movePositionToNextGranularity();
      currentTextIds = chrome.readingMode.getCurrentText();
      hasCurrentText = currentTextIds.length > 0;
      startOfSelectionIsInCurrentText = currentTextIds.includes(nodeId) &&
          chrome.readingMode.getCurrentTextEndIndex(nodeId) > offset;
    }
  }

  highlightAndPlayInterruptedMessage(): boolean {
    return this.highlightAndPlayMessage(/* isInterrupted = */ true);
  }

  // Play text of these axNodeIds. When finished, read and highlight to read the
  // following text.
  // TODO: crbug.com/1474951 - Investigate using AXRange.GetText to get text
  // between start node / end nodes and their offsets.
  highlightAndPlayMessage(
      isInterrupted: boolean = false,
      isMovingBackward: boolean = false): boolean {
    // getCurrentText gets the AX Node IDs of text that should be spoken and
    // highlighted.
    const axNodeIds: number[] = chrome.readingMode.getCurrentText();

    // If there aren't any valid ax node ids returned by getCurrentText,
    // speech should stop.
    if (axNodeIds.length === 0) {
      return false;
    }

    if (this.nodeStore_.areNodesAllHidden(axNodeIds)) {
      return this.skipCurrentPosition_(isInterrupted, isMovingBackward);
    }

    const utteranceText = this.extractTextOf(axNodeIds);
    // If node ids were returned but they don't exist in the Reading Mode panel,
    // there's been a mismatch between Reading Mode and Read Aloud. In this
    // case, we should move to the next Read Aloud node and attempt to continue
    // playing. TODO: crbug.com/332694565 - This fallback should never be
    // needed, but it is. Investigate root cause of Read Aloud / Reading Mode
    // mismatch. Additionally, the TTS engine may not like attempts to speak
    // whitespace, so move to the next utterance in that case.
    if (!utteranceText || utteranceText.trim().length === 0) {
      return this.skipCurrentPosition_(isInterrupted, isMovingBackward);
    }

    // If we're resuming a previously interrupted message, use word
    // boundaries (if available) to resume at the beginning of the current
    // word.
    if (isInterrupted && this.wordBoundaries_.hasBoundaries()) {
      const utteranceTextForWordBoundary =
          utteranceText.substring(this.wordBoundaries_.getResumeBoundary());
      // If we paused right at the end of the sentence, no need to speak the
      // ending punctuation.
      if (this.highlighter_.isInvalidHighlightForWordHighlighting(
              utteranceTextForWordBoundary.trim())) {
        this.wordBoundaries_.resetToDefaultState();
        return this.skipCurrentPosition_(isInterrupted, isMovingBackward);
      } else {
        this.playText(utteranceTextForWordBoundary);
      }
    } else {
      this.playText(utteranceText);
    }

    this.highlightCurrentGranularity(axNodeIds);
    return true;
  }

  private skipCurrentPosition_(
      isInterrupted: boolean, isMovingBackward: boolean): boolean {
    if (isMovingBackward) {
      chrome.readingMode.movePositionToPreviousGranularity();
    } else {
      chrome.readingMode.movePositionToNextGranularity();
    }
    return this.highlightAndPlayMessage(isInterrupted, isMovingBackward);
  }

  // Highlights or rehighlights the current granularity, sentence or word.
  highlightCurrentGranularity(
      axNodeIds: number[], scrollIntoView: boolean = true,
      shouldUpdateSentenceHighlight: boolean = true,
      shouldSetLastReadingPos: boolean = true) {
    if (shouldSetLastReadingPos && axNodeIds.length && axNodeIds[0]) {
      this.lastReadingId_ = axNodeIds[0];
      this.lastReadingOffset_ =
          chrome.readingMode.getCurrentTextStartIndex(this.lastReadingId_);
    }
    this.highlighter_.highlightCurrentGranularity(
        axNodeIds, scrollIntoView, shouldUpdateSentenceHighlight,
        this.selectedVoice_);
  }

  // Gets the accessible text boundary for the given string.
  getAccessibleTextLength(utteranceText: string): number {
    // Splicing on commas won't work for all locales, but since this is a
    // simple strategy for splicing text in languages that do use commas
    // that reduces the need for calling getAccessibleBoundary.
    // TODO(crbug.com/40927698): Investigate if we can utilize comma splices
    // directly in the utils methods called by #getAccessibleBoundary.
    const lastCommaIndex =
        utteranceText.substring(0, MAX_SPEECH_LENGTH).lastIndexOf(', ');

    // To prevent infinite looping, only use the lastCommaIndex if it's not the
    // first character. Otherwise, use getAccessibleBoundary to prevent
    // repeatedly splicing on the first comma of the same substring.
    if (lastCommaIndex > 0) {
      return lastCommaIndex;
    }

    // TODO: crbug.com/40927698 - getAccessibleBoundary breaks on the nearest
    // word boundary, but if there's some type of punctuation (such as a comma),
    // it would be preferable to break on the punctuation so the pause in
    // speech sounds more natural.
    return chrome.readingMode.getAccessibleBoundary(
        utteranceText, MAX_SPEECH_LENGTH);
  }

  private playText(utteranceText: string) {
    // This check is needed due limits of TTS audio for remote voices. See
    // crbug.com/1176078 for more details.
    // Since the TTS bug only impacts remote voices, no need to check for
    // maximum text length if we're using a local voice. If we do somehow
    // attempt to speak text that's too long, this will be able to be handled
    // by listening for a text-too-long error in message.onerror.
    const isTextTooLong = this.selectedVoice_?.localService ?
        false :
        utteranceText.length > MAX_SPEECH_LENGTH;
    const endBoundary = isTextTooLong ?
        this.getAccessibleTextLength(utteranceText) :
        utteranceText.length;
    this.playTextWithBoundaries(utteranceText, isTextTooLong, endBoundary);
  }

  private playTextWithBoundaries(
      utteranceText: string, isTextTooLong: boolean, endBoundary: number) {
    const message =
        new SpeechSynthesisUtterance(utteranceText.substring(0, endBoundary));

    message.onerror = (error) => {
      this.handleSpeechSynthesisError(error, utteranceText);
    };

    message.addEventListener('boundary', (event) => {
      // Some voices may give sentence boundaries, but we're only concerned
      // with word boundaries in boundary event because we're speaking text at
      // the sentence granularity level, so we'll retrieve these boundaries in
      // message.onEnd instead.
      if (event.name === 'word') {
        this.wordBoundaries_.updateBoundary(event.charIndex, event.charLength);

        // No need to update the highlight on word boundary events if
        // highlighting is off or if sentence highlighting is used.
        // Therefore, we don't need to pass in axIds because these are
        // calculated downstream.
        this.highlightCurrentGranularity(
            [], /* scrollIntoView= */ true,
            /*shouldUpdateSentenceHighlight= */ false);
      }
    });

    message.onstart = () => {
      // We've gotten the signal that the speech engine has loaded, therefore
      // we can enable the Read Aloud buttons.
      this.speechEngineLoaded_ = true;

      // Reset the isSpeechBeingRepositioned property after speech starts
      // after a next / previous button.
      this.speechController_.setIsSpeechBeingRepositioned(false);
      this.speechController_.setIsAudioCurrentlyPlaying(true);
    };

    message.onend = () => {
      if (isTextTooLong) {
        // Since our previous utterance was too long, continue speaking pieces
        // of the current utterance until the utterance is complete. The
        // entire utterance is highlighted, so there's no need to update
        // highlighting until the utterance substring is an acceptable size.
        this.playText(utteranceText.substring(endBoundary));
        return;
      }

      // Now that we've finiished reading this utterance, update the
      // Granularity state to point to the next one Reset the word boundary
      // index whenever we move the granularity position.
      this.wordBoundaries_.resetToDefaultState();
      chrome.readingMode.movePositionToNextGranularity();
      // Continue speaking with the next block of text.
      if (!this.highlightAndPlayMessage()) {
        this.onSpeechFinished();
      }
    };

    const voice = this.getSpeechSynthesisVoice();
    if (!voice) {
      // TODO: crbug.com/40927698 - Handle when no voices are available.
      return;
    }

    // This should only be false in tests where we can't properly construct an
    // actual SpeechSynthesisVoice object even though the test voices pass the
    // type checking of method signatures.
    if (voice instanceof SpeechSynthesisVoice) {
      message.voice = voice;
    }

    const utteranceSettings = this.defaultUtteranceSettings();
    message.lang = utteranceSettings.lang;
    message.volume = utteranceSettings.volume;
    message.pitch = utteranceSettings.pitch;
    message.rate = utteranceSettings.rate;


    if (!this.firstUtteranceSpoken_) {
      this.speechEngineLoaded_ = false;
      this.firstUtteranceSpoken_ = true;
    }
    this.speech_.speak(message);
  }

  handleSpeechSynthesisError(
      error: SpeechSynthesisErrorEvent, utteranceText: string) {
    // We can't be sure that the engine has loaded at this point, but
    // if there's an error, we want to ensure we keep the play buttons
    // to prevent trapping users in a state where they can no longer play
    // Read Aloud, as this is preferable to a long delay before speech
    // with no feedback.
    this.speechEngineLoaded_ = true;

    if (error.error === 'interrupted') {
      this.speechController_.onSpeechInterrupted();
      return;
    }

    // Log a speech error. We aren't concerned with logging an interrupted
    // error, since that can be triggered from play / pause.
    this.logger_.logSpeechError(error.error);

    if (error.error === 'text-too-long') {
      // This is unlikely to happen, as the length limit on most voices
      // is quite long. However, if we do hit a limit, we should just use
      // the accessible text length boundaries to shorten the text. Even
      // if this gives a much smaller sentence than TTS would have supported,
      // this is still preferable to no speech.
      this.speech_.cancel();
      this.playTextWithBoundaries(
          utteranceText, true, this.getAccessibleTextLength(utteranceText));
      return;
    }
    if (error.error === 'invalid-argument') {
      // invalid-argument can be triggered when the rate, pitch, or volume
      // is not supported by the synthesizer. Since we're only setting the
      // speech rate, update the speech rate to the WebSpeech default of 1.
      chrome.readingMode.onSpeechRateChange(1);
      this.resetSpeechPostSettingChange_();
      return;
    }

    // No appropriate voice is available for the language designated in
    // SpeechSynthesisUtterance lang.
    if (error.error === 'language-unavailable') {
      const possibleNewLanguage = convertLangToAnAvailableLangIfPresent(
          this.speechSynthesisLanguage,
          this.voicePackController_.getAvailableLangs(),
          /* allowCurrentLanguageIfExists */ false);
      if (possibleNewLanguage) {
        this.speechSynthesisLanguage = possibleNewLanguage;
      }
    }

    // The voice designated in SpeechSynthesisUtterance voice attribute
    // is not available.
    if (error.error === 'voice-unavailable') {
      let newVoice = this.selectedVoice_ ? this.selectedVoice_ : undefined;
      this.selectedVoice_ = undefined;
      newVoice = this.getAlternativeVoice(newVoice);

      if (newVoice) {
        this.selectedVoice_ = newVoice;
      }
    }

    // When we hit an error, stop speech to clear all utterances, update the
    // button state, and highlighting in order to give visual feedback that
    // something went wrong.
    // TODO: crbug.com/40927698 - Consider showing an error message.
    this.logger_.logSpeechStopSource(chrome.readingMode.engineErrorStopSource);
    this.speechController_.stopSpeech(PauseActionSource.DEFAULT);
  }

  private extractTextOf(axNodeIds: number[]): string {
    let utteranceText: string = '';
    for (const nodeId of axNodeIds) {
      const startIndex = chrome.readingMode.getCurrentTextStartIndex(nodeId);
      const endIndex = chrome.readingMode.getCurrentTextEndIndex(nodeId);
      const element = this.nodeStore_.getDomNode(nodeId);
      if (!element || startIndex < 0 || endIndex < 0) {
        continue;
      }
      const content = chrome.readingMode.getTextContent(nodeId).substring(
          startIndex, endIndex);
      if (content) {
        // Add all of the text from the current nodes into a single utterance.
        utteranceText += content;
      }
    }
    return utteranceText;
  }

  private defaultUtteranceSettings(): UtteranceSettings {
    const lang = this.speechSynthesisLanguage;

    return {
      lang,
      // TODO: crbug.com/40927698 - Ensure the rate is valid for the current
      // speech engine.
      rate: getCurrentSpeechRate(),
      volume: 1,
      pitch: 1,
    };
  }

  private onSpeechFinished() {
    this.logger_.logSpeechStopSource(
        chrome.readingMode.contentFinishedStopSource);
    this.clearReadAloudState();

    // Show links when speech finishes playing.
    if (chrome.readingMode.linksEnabled) {
      this.updateLinks_();
    }
    this.logSpeechPlaySession_();
  }

  private clearReadAloudState() {
    this.speechController_.reset();
    this.highlighter_.clearHighlightFormatting();
    this.wordBoundaries_.resetToDefaultState();
  }

  protected onSelectVoice_(
      event: CustomEvent<{selectedVoice: SpeechSynthesisVoice}>) {
    event.preventDefault();
    event.stopPropagation();

    let localesAreIdentical = false;
    if (this.selectedVoice_) {
      localesAreIdentical = this.selectedVoice_.lang.toLowerCase() ===
          event.detail.selectedVoice.lang.toLowerCase();
    }

    this.selectedVoice_ = event.detail.selectedVoice;
    chrome.readingMode.onVoiceChange(
        this.selectedVoice_.name, this.selectedVoice_.lang);

    // If the locales are identical, the voices are likely from the same
    // voice pack and use the same TTS engine, therefore, we don't need
    // to reset the word boundary state.
    if (!localesAreIdentical) {
      this.wordBoundaries_.resetToDefaultState(
          /*possibleWordBoundarySupportChange=*/ true);
    }

    this.resetSpeechPostSettingChange_();
  }

  protected onVoiceLanguageToggle_(event: CustomEvent<{language: string}>) {
    event.preventDefault();
    event.stopPropagation();
    const toggledLanguage = event.detail.language;
    const currentlyEnabled =
        this.voicePackController_.isLangEnabled(toggledLanguage);

    if (!currentlyEnabled) {
      this.autoSwitchVoice_(toggledLanguage);
      this.installVoicePackIfPossible(
          toggledLanguage, /* onlyInstallExactGoogleLocaleMatch=*/ true,
          /* retryIfPreviousInstallFailed= */ true);
    } else {
      this.voicePackController_.uninstall(toggledLanguage);
    }

    const updateEnabledLangs = currentlyEnabled ?
        this.voicePackController_.disableLang(toggledLanguage) :
        this.voicePackController_.enableLang(toggledLanguage);
    if (updateEnabledLangs) {
      this.enabledLangs_ = this.voicePackController_.getEnabledLangs();
    }

    chrome.readingMode.onLanguagePrefChange(toggledLanguage, !currentlyEnabled);

    if (!currentlyEnabled && !this.selectedVoice_) {
      // If there were no enabled languages (and thus no selected voice),
      // select a voice.
      this.getSpeechSynthesisVoice();
    }
  }

  protected resetSpeechPostSettingChange_() {
    // Don't call stopSpeech() if the speech tree hasn't been initialized or
    // if speech hasn't been triggered yet.
    if (!this.speechController_.isSpeechTreeInitialized() ||
        !this.speechController_.hasSpeechBeenTriggered()) {
      return;
    }

    const playSpeechOnChange = this.speechController_.isSpeechActive();

    // Cancel the queued up Utterance using the old speech settings
    this.speechController_.stopSpeech(PauseActionSource.VOICE_SETTINGS_CHANGE);

    // If speech was playing when a setting was changed, continue playing
    // speech
    if (playSpeechOnChange) {
      this.playSpeech();
    }
  }

  restoreSettingsFromPrefs() {
    if (this.isReadAloudEnabled_) {
      // We need to restore enabled languages prior to selecting the preferred
      // voice to ensure we have the right voices available.
      this.restoreEnabledLanguagesFromPref();
      this.selectPreferredVoice();
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

  restoreEnabledLanguagesFromPref() {
    // We need to make sure the languages we choose correspond to voices, so
    // refresh the list of voices and available langs
    this.getVoices_();

    this.speechSynthesisLanguage = chrome.readingMode.baseLanguageForSpeech;
    this.enabledLangs_ =
        this.voicePackController_.getInitialListOfEnabledLanguages(
            this.defaultVoice()?.lang);

    for (const lang of this.enabledLangs_) {
      this.installVoicePackIfPossible(
          lang, /* onlyInstallExactGoogleLocaleMatch=*/ true,
          /* retryIfPreviousInstallFailed= */ false);
    }
  }

  private currentVoiceIsUserChosen_(): boolean {
    const storedVoiceName = chrome.readingMode.getStoredVoice();

    // `this.selectedVoice` is not necessarily chosen by the user, it is just
    // the voice that read aloud is using. It may be a default voice chosen by
    // read aloud, so we check it against user preferences to see if it was
    // user-chosen.
    if (storedVoiceName) {
      return this.selectedVoice_?.name === storedVoiceName;
    }
    return false;
  }

  selectPreferredVoice() {
    // TODO: crbug.com/40275871 - decide whether this is the behavior we want.
    // This shouldn't happen often, so just skip selecting a new voice for now.
    // Another option would be to update the voice and the call
    // resetSpeechPostSettingsChange(), but that could be jarring.
    if (this.speechController_.hasSpeechBeenTriggered()) {
      return;
    }

    const storedVoiceName = chrome.readingMode.getStoredVoice();
    if (!storedVoiceName) {
      this.selectedVoice_ = this.defaultVoice();
      return;
    }

    const selectedVoice =
        this.getVoices_().filter(voice => voice.name === storedVoiceName);
    this.selectedVoice_ = selectedVoice && (selectedVoice.length > 0) ?
        selectedVoice[0] :
        this.defaultVoice();

    // Enable the locale for the preferred voice for this language.
    if (this.voicePackController_.enableLang(this.selectedVoice_?.lang)) {
      this.enabledLangs_ = this.voicePackController_.getEnabledLangs();
    }
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
    // Handler for HIGHLIGHT_CHANGE.
    const changedHighlight = event.detail.data;
    chrome.readingMode.onHighlightGranularityChanged(changedHighlight);
    // Apply highlighting changes to the DOM.
    this.styleUpdater_.setHighlight();

    // Rehighlight the new granularity.
    if (changedHighlight !== chrome.readingMode.noHighlighting) {
      this.highlightCurrentGranularity(chrome.readingMode.getCurrentText());
    }

    // Log these highlight granularity changes when the phrase menu is shown.
    // (Toggles are already logged in the toolbar.)
    this.logger_.logHighlightGranularity(changedHighlight);
  }

  // If the screen is locked during speech, we should stop speaking.
  onLockScreen() {
    if (this.speechController_.isSpeechActive()) {
      this.speechController_.stopSpeech(PauseActionSource.DEFAULT);
    }
  }

  onTtsEngineInstalled() {
    this.waitingForNewEngine_ = true;
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
    this.speechSynthesisLanguage = chrome.readingMode.baseLanguageForSpeech;
    this.$.toolbar.updateFonts();
    // Don't check for Google locales when the language has changed.
    this.installVoicePackIfPossible(
        this.speechSynthesisLanguage,
        /* onlyInstallExactGoogleLocaleMatch=*/ false,
        /* retryIfPreviousInstallFailed= */ false);
  }

  protected computeIsReadAloudPlayable(): boolean {
    return this.hasContent_ && this.speechEngineLoaded_ &&
        !!this.selectedVoice_ && !this.willDrawAgainSoon_;
  }

  private autoSwitchVoice_(lang: string) {
    // Only enable this language if it has available voices and is the current
    // language. Otherwise switch to a default voice if nothing is selected.
    const availableLang = convertLangToAnAvailableLangIfPresent(
        lang, this.voicePackController_.getAvailableLangs());
    const speechSynthesisBaseLang = this.speechSynthesisLanguage.split('-')[0];
    if (!availableLang ||
        (speechSynthesisBaseLang &&
         !availableLang.startsWith(speechSynthesisBaseLang))) {
      this.selectPreferredVoice();
      return;
    }

    // Enable the preferred locale for this lang if one exists. Otherwise,
    // enable a Google TTS supported locale for this language if one exists.
    const preferredVoice = chrome.readingMode.getStoredVoice();
    const preferredVoiceLang =
        this.getVoices_().find(voice => voice.name === preferredVoice)?.lang;
    let localeToEnable: string|undefined = preferredVoiceLang ?
        preferredVoiceLang :
        convertLangOrLocaleToExactVoicePackLocale(availableLang);

    // If there are no Google TTS locales for this language then enable the
    // first available locale for this language.
    if (!localeToEnable) {
      localeToEnable = this.voicePackController_.getAvailableLangs().find(
          l => l.startsWith(availableLang));
    }

    // Enable the locales so we can select a voice for the given language and
    // show it in the voice menu.
    if (this.voicePackController_.enableLang(localeToEnable)) {
      this.enabledLangs_ = this.voicePackController_.getEnabledLangs();
    }
    this.selectPreferredVoice();
  }

  // Kicks off a workflow to install a voice pack.
  // 1) Checks if Language Pack Manager supports a version of this
  // voice/locale 2) If so, adds voice to installVoicePackIfPossible set 3)
  // Kicks off request GetVoicePackInfo to see if the voice is installed 4)
  // Upon response, if we see the voice is not installed and that it's in
  // installVoicePackIfPossible, then we trigger an install request
  private installVoicePackIfPossible(
      langOrLocale: string, onlyInstallExactGoogleLocaleMatch: boolean,
      retryIfPreviousInstallFailed: boolean) {
    // Don't attempt to install a language if it's not a Google TTS language
    // available for downloading. It's possible for other non-Google TTS
    // voices to have a valid language code from
    // convertLangOrLocaleForVoicePackManager, so return early instead to
    // prevent accidentally downloading untoggled voices.
    // If we shouldn't check for Google locales (such as when installing a new
    // page language), this check can be skipped.
    if (onlyInstallExactGoogleLocaleMatch &&
        !AVAILABLE_GOOGLE_TTS_LOCALES.has(langOrLocale)) {
      this.autoSwitchVoice_(langOrLocale);
      return;
    }

    const langCodeForVoicePackManager = convertLangOrLocaleForVoicePackManager(
        langOrLocale, this.voicePackController_.getEnabledLangs(),
        this.voicePackController_.getAvailableLangs());

    if (!langCodeForVoicePackManager) {
      this.autoSwitchVoice_(langOrLocale);
      return;
    }

    if (!this.voicePackController_.requestInstall(
            langCodeForVoicePackManager, retryIfPreviousInstallFailed)) {
      this.autoSwitchVoice_(langCodeForVoicePackManager);
    }
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

  resetVoiceForTesting() {
    this.selectedVoice_ = undefined;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'read-anything-app': AppElement;
  }
}

customElements.define(AppElement.is, AppElement);
