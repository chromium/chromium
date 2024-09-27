// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './read_anything_toolbar.js';
import './strings.m.js';
import '//read-anything-side-panel.top-chrome/shared/sp_empty_state.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_toast/cr_toast.js';
import './language_toast.js';

import {ColorChangeUpdater} from '//resources/cr_components/color_change_listener/colors_css_updater.js';
import {WebUiListenerMixinLit} from '//resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {listenOnce} from '//resources/js/util.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {AppStyleUpdater} from './app_style_updater.js';
import type {SettingsPrefs} from './common.js';
import {getCurrentSpeechRate, minOverflowLengthToScroll, playFromSelectionTimeout} from './common.js';
import type {LanguageToastElement} from './language_toast.js';
import {ReadAnythingLogger, TimeFrom, TimeTo} from './read_anything_logger.js';
import type {ReadAnythingToolbarElement} from './read_anything_toolbar.js';
import {areVoicesEqual, AVAILABLE_GOOGLE_TTS_LOCALES, convertLangOrLocaleForVoicePackManager, convertLangOrLocaleToExactVoicePackLocale, convertLangToAnAvailableLangIfPresent, createInitialListOfEnabledLanguages, doesLanguageHaveNaturalVoices, getFilteredVoiceList, getNaturalVoiceOrDefault, getVoicePackConvertedLangIfExists, isEspeak, isNatural, isVoicePackStatusError, isVoicePackStatusSuccess, mojoVoicePackStatusToVoicePackStatusEnum, VoiceClientSideStatusCode, VoicePackServerStatusErrorCode, VoicePackServerStatusSuccessCode} from './voice_language_util.js';
import type {VoicePackStatus} from './voice_language_util.js';
import {VoiceNotificationManager} from './voice_notification_manager.js';

const AppElementBase = WebUiListenerMixinLit(CrLitElement);

interface UtteranceSettings {
  lang: string;
  volume: number;
  pitch: number;
  rate: number;
}

export const previousReadHighlightClass = 'previous-read-highlight';
export const currentReadHighlightClass = 'current-read-highlight';
const parentOfHighlightClass = 'parent-of-highlight';

const linkDataAttribute = 'link';

// Characters that should be ignored for word highlighting when not accompanied
// by other characters.
const IGNORED_HIGHLIGHT_CHARACTERS_REGEX: RegExp = /^[.,!?'"(){}\[\]]+$/;

// A two-way map where each key is unique and each value is unique. The keys are
// DOM nodes and the values are numbers, representing AXNodeIDs.
class TwoWayMap<K, V> extends Map<K, V> {
  #reverseMap: Map<V, K>;
  constructor() {
    super();
    this.#reverseMap = new Map();
  }
  override set(key: K, value: V) {
    super.set(key, value);
    this.#reverseMap.set(value, key);
    return this;
  }
  keyFrom(value: V) {
    return this.#reverseMap.get(value);
  }
  override clear() {
    super.clear();
    this.#reverseMap.clear();
  }
}

export enum PauseActionSource {
  DEFAULT,
  BUTTON_CLICK,
  VOICE_PREVIEW,
  VOICE_SETTINGS_CHANGE,
}

export enum WordBoundaryMode {
  // Used if word boundaries are not supported (i.e. we haven't received enough
  // information to determine if word boundaries are supported.)
  BOUNDARIES_NOT_SUPPORTED,
  NO_BOUNDARIES,
  BOUNDARY_DETECTED,
}

export interface SpeechPlayingState {
  // If the speech tree for the current page has been initialized. This happens
  // in updateContent before speech has been initiated by users but it can
  // also be set to true via a play from selection.
  isSpeechTreeInitialized: boolean;
  // True when the user presses play, regardless of if audio has actually
  // started yet. This will be false when speech is paused.
  isSpeechActive: boolean;
  // When `isSpeechActive` is false, this indicates how it became false. e.g.
  // via pause button click or because other speech settings were changed.
  pauseSource?: PauseActionSource;
  // Indicates that audio is currently playing.
  // When a user presses the play button, isSpeechActive becomes true, but
  // `isAudioCurrentlyPlaying` will tell us whether audio actually started
  // playing yet. This is a separate state because audio starting has a delay.
  isAudioCurrentlyPlaying: boolean;
  // Indicates if speech has been triggered on the current page by a play
  // button press. This will be true throughout the lifetime of reading
  // the content on the page. It will only be reset when speech has completely
  // stopped from reaching the end of content or changing pages. Pauses will
  // not update it.
  hasSpeechBeenTriggered: boolean;
}

export interface WordBoundaryState {
  mode: WordBoundaryMode;
  // The charIndex of the last word boundary index retrieved by the "Boundary"
  // event. Default is 0.
  previouslySpokenIndex: number;
  // Is only non-zero if the current state has already resumed speech on a
  // word boundary. e.g. If we interrupted speech for the segment
  // "This is a sentence" at "is," so the next segment spoken is "is a
  // sentence," if we attempt to interrupt speech again at "a." This helps us
  // keep track of the correct index in the overall granularity string- not
  // just the correct index within the current string.
  // Default is 0.
  speechUtteranceStartIndex: number;
}

export interface AppElement {
  $: {
    toolbar: ReadAnythingToolbarElement,
    appFlexParent: HTMLElement,
    container: HTMLElement,
    containerParent: HTMLElement,
    languageToast: LanguageToastElement,
  };
}

function isInvalidHighlightForWordHighlighting(textToHighlight: string|
                                               undefined): boolean {
  // If a highlight is just white space or punctuation, we can skip
  // highlighting.
  return !textToHighlight || textToHighlight === '' ||
      IGNORED_HIGHLIGHT_CHARACTERS_REGEX.test(textToHighlight);
}

export class AppElement extends AppElementBase {
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
      speechPlayingState: {type: Object},
      imagesEnabled: {type: Boolean, reflect: true},
      enabledLangs: {type: Array},
      settingsPrefs_: {type: Object},
      selectedVoice_: {type: Object},
      availableVoices_: {type: Array},
      voiceStatusLocalState_: {type: Object},
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

  // Maps a DOM node to the AXNodeID that was used to create it. DOM nodes and
  // AXNodeIDs are unique, so this is a two way map where either DOM node or
  // AXNodeID can be used to access the other.
  private domNodeToAxNodeIdMap_: TwoWayMap<Node, number> = new TwoWayMap();
  // Key: a DOM node that's already been read aloud
  // Value: the index offset at which this node's text begins within its parent
  // text. For reading aloud we sometimes split up nodes so the speech sounds
  // more natural. When that text is then selected we need to pass the correct
  // index down the pipeline, so we store that info here.
  private highlightedNodeToOffsetInParent: Map<Node, number> = new Map();
  private imageNodeIdsToFetch_: Set<number> = new Set();

  private scrollingOnSelection_ = false;
  protected hasContent_ = false;
  protected emptyStateImagePath_?: string;
  protected emptyStateDarkImagePath_?: string;
  protected emptyStateHeading_?: string;
  protected emptyStateSubheading_ = '';

  private previousHighlights_: HTMLElement[] = [];
  private previousRootId_: number;

  private isReadAloudEnabled_: boolean;
  protected isDocsLoadMoreButtonVisible_: boolean = false;

  // If the speech engine is considered "loaded." If it is, we should display
  // the play / pause buttons normally. Otherwise, we should disable the
  // Read Aloud controls until the engine has loaded in order to provide
  // visual feedback that a voice is about to be spoken.
  private speechEngineLoaded_: boolean = true;

  // Sometimes distillations are queued up while distillation is happening so
  // when the current distillation finishes, we re-distill immediately. In that
  // case we shouldn't allow playing speech until the next distillation to avoid
  // resetting speech right after starting it.
  private willDrawAgainSoon_: boolean = false;

  // After the first utterance has been spoken, we should assume that the
  // speech engine has loaded, and we shouldn't adjust the play / pause
  // disabled state based on the message.onStart callback to avoid flickering.
  private firstUtteranceSpoken_ = false;

  synth = window.speechSynthesis;

  protected selectedVoice_: SpeechSynthesisVoice|undefined;
  // The set of languages currently enabled for use by Read Aloud. This
  // includes user-enabled languages and auto-downloaded languages. The former
  // are stored in preferences. The latter are not.
  enabledLangs: string[] = [];

  // All possible available voices for the current speech engine.
  protected availableVoices_: SpeechSynthesisVoice[] = [];
  // The set of languages found in availableVoices.
  private availableLangs_: string[] = [];
  // If a preview is playing, this is set to the voice the preview is playing.
  // Otherwise, this is undefined.
  protected previewVoicePlaying_?: SpeechSynthesisVoice;

  protected localeToDisplayName_: {[locale: string]: string};

  // Our local representation of the status of voice pack downloads and
  // availability
  private voiceStatusLocalState_:
      {[language: string]: VoiceClientSideStatusCode} = {};

  // Cache of responses from LanguagePackManager
  private voicePackInstallStatusServerResponses_:
      {[language: string]: VoicePackStatus} = {};

  // Set of languages of the browser and/or of the pages navigated to that we
  // need to download Natural voices for automatically
  private languagesForVoiceDownloads: Set<string> = new Set();

  // Metrics captured for logging.
  private playSessionStartTime: number = -1;

  private notificationManager_: VoiceNotificationManager;
  private logger_: ReadAnythingLogger = ReadAnythingLogger.getInstance();
  private styleUpdater_: AppStyleUpdater;
  protected settingsPrefs_: SettingsPrefs;

  // State for speech synthesis paused/play state needs to be tracked explicitly
  // because there are bugs with window.speechSynthesis.paused and
  // window.speechSynthesis.speaking on some platforms.
  speechPlayingState: SpeechPlayingState = {
    isSpeechTreeInitialized: false,
    isSpeechActive: false,
    pauseSource: PauseActionSource.DEFAULT,
    isAudioCurrentlyPlaying: false,
    hasSpeechBeenTriggered: false,
  };

  private imagesEnabled: boolean = false;

  maxSpeechLength: number = 175;

  wordBoundaryState: WordBoundaryState = {
    mode: WordBoundaryMode.BOUNDARIES_NOT_SUPPORTED,
    speechUtteranceStartIndex: 0,
    previouslySpokenIndex: 0,
  };

  // If the node id of the first text node that should be used by Read Aloud
  // has been set. This is null if the id has not been set.
  firstTextNodeSetForReadAloud: number|null = null;

  speechSynthesisLanguage: string;

  // If we weren't able to restore language preferences successfully and we
  // should attempt to restore settings if voices refresh.
  // Sometimes, the speech synthesis engine hasn't refreshed available
  // voices by the time we restore settings, which means we end up
  // ignoring previous settings if we get an onvoiceschanged callback
  // a few seconds later. By keeping track of whether or not preferences
  // were successfully restored, we can re-attempt to restore voice and
  // language preferences from settings in onVoicesChanged.
  shouldAttemptLanguageSettingsRestore: boolean = true;


  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('speechPlayingState')) {
      chrome.readingMode.onSpeechPlayingStateChanged(
          this.speechPlayingState.isSpeechActive);
    }
  }

  constructor() {
    super();
    this.constructorTime = Date.now();
    this.logger_.logTimeBetween(
        TimeFrom.APP, TimeTo.CONSTRUCTOR, this.startTime, this.constructorTime);
    this.isReadAloudEnabled_ = chrome.readingMode.isReadAloudEnabled;
    this.speechSynthesisLanguage = chrome.readingMode.baseLanguageForSpeech;
    this.styleUpdater_ = new AppStyleUpdater(this);
    this.notificationManager_ = VoiceNotificationManager.getInstance();
    ColorChangeUpdater.forDocument().start();
  }

  override connectedCallback() {
    super.connectedCallback();

    // onConnected should always be called first in connectedCallback to ensure
    // we're not blocking onConnected on anything else during WebUI setup.
    if (chrome.readingMode) {
      chrome.readingMode.onConnected();
      const connectedCallbackTime = Date.now();
      this.logger_.logTimeBetween(
          TimeFrom.APP, TimeTo.CONNNECTED_CALLBACK, this.startTime,
          connectedCallbackTime);
      this.logger_.logTimeBetween(
          TimeFrom.APP_CONSTRUCTOR, TimeTo.CONNNECTED_CALLBACK,
          this.constructorTime, connectedCallbackTime);
    }

    // Wait until the side panel is fully rendered before showing the side
    // panel. This follows Side Panel best practices and prevents loading
    // artifacts from showing if the side panel is shown before content is
    // ready.
    listenOnce(this.$.appFlexParent, 'dom-change', () => {
      setTimeout(() => chrome.readingMode.shouldShowUi(), 0);
    });

    this.showLoading();
    VoiceNotificationManager.getInstance().addListener(this.$.languageToast);

    if (this.isReadAloudEnabled_) {
      // Clear state. We don't do this in disconnectedCallback because that's
      // not always reliabled called.
      this.synth.cancel();
      this.hasContent_ = false;
      this.firstUtteranceSpoken_ = false;
      this.firstTextNodeSetForReadAloud = null;
      this.domNodeToAxNodeIdMap_.clear();
      this.clearReadAloudState();

      this.synth.onvoiceschanged = () => {
        this.onVoicesChanged();
      };
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
      if (!this.hasContent_ || this.speechPlayingState.isSpeechActive) {
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

      chrome.readingMode.onSelectionChange(
          anchorNodeId, anchorOffset, focusNodeId, focusOffset);
      // If there's been a selection, clear the current
      // Read Aloud highlight.
      const elements =
          this.shadowRoot?.querySelectorAll('.' + currentReadHighlightClass);
      if (elements && anchorNodeId && focusNodeId) {
        elements.forEach(el => el.classList.remove(currentReadHighlightClass));
      }

      // Clear the previously read highlight if there's been a selection.
      // If speech is resumed, this won't be restored.
      // TODO(b/40927698): Restore the previous highlight after speech
      // is resumed after a selection.
      this.previousHighlights_.forEach((element) => {
        if (element) {
          element.classList.remove(previousReadHighlightClass);
        }
      });
      this.previousHighlights_ = [];
    };

    this.$.containerParent.onscroll = () => {
      chrome.readingMode.onScroll(this.scrollingOnSelection_);
      this.scrollingOnSelection_ = false;
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

    chrome.readingMode.updateVoicePackStatusFromInstallResponse =
        (lang: string, status: string) => {
          this.updateVoicePackStatusFromInstallResponse(lang, status);
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
  }

  private getOffsetInAncestor(node: Node): number {
    if (this.highlightedNodeToOffsetInParent.has(node)) {
      return this.highlightedNodeToOffsetInParent.get(node)!;
    }

    return 0;
  }

  private getHighlightedAncestorId_(node: Node): number|undefined {
    if (!node.parentElement || !node.parentNode) {
      return undefined;
    }

    let ancestor;
    if (node.parentElement.classList.contains(parentOfHighlightClass)) {
      ancestor = node.parentNode;
    } else if (node.parentElement.parentElement?.classList.contains(
                   parentOfHighlightClass)) {
      ancestor = node.parentNode.parentNode;
    }

    return ancestor ? this.domNodeToAxNodeIdMap_.get(ancestor) : undefined;
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
    this.domNodeToAxNodeIdMap_.set(element, nodeId);
    const direction = chrome.readingMode.getTextDirection(nodeId);
    if (direction) {
      element.setAttribute('dir', direction);
    }

    if (element.nodeName === 'CANVAS') {
      this.imageNodeIdsToFetch_.add(nodeId);
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

  // TODO(crbug.com/40910704): Potentially hide links during distillation.
  private shouldShowLinks(): boolean {
    // Links should only show when Read Aloud is paused.
    return chrome.readingMode.linksEnabled &&
        !this.speechPlayingState.isSpeechActive;
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
      this.initializeSpeechTree();
    }

    const textContent = chrome.readingMode.getTextContent(nodeId);
    const textNode = document.createTextNode(textContent);
    this.domNodeToAxNodeIdMap_.set(textNode, nodeId);
    const isOverline = chrome.readingMode.isOverline(nodeId);
    let shouldBold = chrome.readingMode.shouldBold(nodeId);

    if (chrome.readingMode.isGoogleDocs) {
      const dataFontCss = chrome.readingMode.getDataFontCss(nodeId);
      if (dataFontCss) {
        const styleNode = document.createElement('style');
        styleNode.style.cssText = `font:${dataFontCss}`;
        if (styleNode.style.fontStyle === 'italic') {
          shouldBold = true;
        }
        const fontWeight = +styleNode.style.fontWeight;
        if (!isNaN(fontWeight) && fontWeight > 500) {
          shouldBold = true;
        }
      }
    }

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
    if (chrome.readingMode.isGoogleDocs) {
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
      this.synth.cancel();
      this.clearReadAloudState();
    }
  }

  // TODO(crbug.com/40927698): Handle focus changes for speech, including
  // updating speech state.
  updateContent() {
    // Each time we rebuild the subtree, we should clear the node id of the
    // first text node.
    this.firstTextNodeSetForReadAloud = null;
    this.synth.cancel();
    this.clearReadAloudState();
    const container = this.$.container;

    // Remove all children from container. Use `replaceChildren` rather than
    // setting `innerHTML = ''` in order to remove all listeners, too.
    container.replaceChildren();
    this.domNodeToAxNodeIdMap_.clear();

    // Construct a dom subtree starting with the display root and append it to
    // the container. The display root may be invalid if there are no content
    // nodes and no selection.
    // This does not use polymer's templating abstraction, which
    // would create a shadow node element representing each AXNode, because
    // experimentation found the shadow node creation to be ~8-10x slower than
    // constructing and appending nodes directly to the container element.
    const rootId = chrome.readingMode.rootId;
    if (!rootId) {
      return;
    }

    this.willDrawAgainSoon_ = chrome.readingMode.requiresDistillation;
    const node = this.buildSubtree_(rootId);
    if (!node.textContent) {
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
  }

  async onImageDownloaded(nodeId: number) {
    const data = chrome.readingMode.getImageBitmap(nodeId);
    const element = this.domNodeToAxNodeIdMap_.keyFrom(nodeId);
    if (data && element && element instanceof HTMLCanvasElement) {
      element.width = data.width;
      element.height = data.height;
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

  private sendGetVoicePackInfoRequest(langOrLocale: string) {
    const langOrLocaleForPackManager =
        convertLangOrLocaleForVoicePackManager(langOrLocale);
    if (langOrLocaleForPackManager) {
      chrome.readingMode.sendGetVoicePackInfoRequest(
          langOrLocaleForPackManager);
    }
  }

  private async loadImages_() {
    if (!chrome.readingMode.imagesFeatureEnabled) {
      return;
    }

    for (const nodeId of this.imageNodeIdsToFetch_) {
      chrome.readingMode.requestImageData(nodeId);
    }

    this.imageNodeIdsToFetch_.clear();
  }

  getSelection(): any {
    assert(this.shadowRoot, 'no shadow root');
    return this.shadowRoot.getSelection();
  }

  updateSelection() {
    const selection: Selection = this.getSelection()!;
    selection.removeAllRanges();

    const range = new Range();
    const startNodeId = chrome.readingMode.startNodeId;
    const endNodeId = chrome.readingMode.endNodeId;
    let startOffset = chrome.readingMode.startOffset;
    let endOffset = chrome.readingMode.endOffset;
    let startNode = this.domNodeToAxNodeIdMap_.keyFrom(startNodeId);
    let endNode = this.domNodeToAxNodeIdMap_.keyFrom(endNodeId);
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

    const originallyHadHighlights =
        this.shadowRoot
            .querySelectorAll<HTMLElement>('.' + currentReadHighlightClass)
            .length > 0;

    const selector = this.shouldShowLinks() ? 'span[data-link]' : 'a';
    const elements = this.shadowRoot.querySelectorAll(selector);

    for (const elem of elements) {
      assert(elem instanceof HTMLElement, 'link is not an HTMLElement');
      const nodeId = this.domNodeToAxNodeIdMap_.get(elem);
      assert(nodeId !== undefined, 'link node id is undefined');
      const replacement = this.buildSubtree_(nodeId);
      this.replaceElement(elem, replacement);
    }

    // Rehighlight the current granularity text after links have been
    // toggled on or off to ensure the entire granularity segment is
    // highlighted.
    if (shouldRehighlightCurrentNodes && originallyHadHighlights) {
      this.highlightCurrentGranularity(chrome.readingMode.getCurrentText());
    }
  }

  protected updateImages_() {
    if (!this.shadowRoot) {
      return;
    }

    this.imagesEnabled = chrome.readingMode.imagesEnabled;
    // There is some strange issue where the HTML css application does not work
    // on canvases.
    for (const canvas of this.shadowRoot.querySelectorAll('canvas')) {
      canvas.style.display = this.imagesEnabled ? '' : 'none';
    }
    for (const canvas of this.shadowRoot.querySelectorAll('figure')) {
      canvas.style.display = this.imagesEnabled ? '' : 'none';
    }
  }

  protected onDocsLoadMoreButtonClick_() {
    chrome.readingMode.onScrolledToBottom();
  }

  updateVoicePackStatusFromInstallResponse(lang: string, status: string) {
    if (!lang) {
      return;
    }

    const newVoicePackStatus = mojoVoicePackStatusToVoicePackStatusEnum(status);

    if (isVoicePackStatusError(newVoicePackStatus)) {
      // Keep the server responses.
      this.setVoicePackServerStatus_(lang, newVoicePackStatus);

      // Update application state.
      this.updateApplicationState(lang, newVoicePackStatus);

      // Disable the associated language if there are no other Google voices for
      // it.
      const availableVoicesForLang = this.getVoices_().filter(
          v => getVoicePackConvertedLangIfExists(v.lang) === lang);
      if (availableVoicesForLang.length === 0 ||
          availableVoicesForLang.every(v => isEspeak(v))) {
        this.enabledLangs = this.enabledLangs.filter(
            enabledLang =>
                getVoicePackConvertedLangIfExists(enabledLang) !== lang);
      }
    } else {
      // Do not rely on the status from Install response. It has responded
      // "installed" for voices that are not installed. Instead, request the
      // status from GetVoicePackInfo. The result will be returned in
      // updateVoicePackStatus().
      this.sendGetVoicePackInfoRequest(lang);
    }
  }

  updateVoicePackStatus(lang: string, status: string) {
    if (!lang) {
      return;
    }

    const newVoicePackStatus = mojoVoicePackStatusToVoicePackStatusEnum(status);

    // Keep the server responses
    this.setVoicePackServerStatus_(lang, newVoicePackStatus);

    // Update application state
    this.updateApplicationState(lang, newVoicePackStatus);
  }


  // Store client side voice pack state and trigger side effects
  private updateApplicationState(
      lang: string, newVoicePackStatus: VoicePackStatus) {
    if (isVoicePackStatusSuccess(newVoicePackStatus)) {
      const newStatusCode = newVoicePackStatus.code;

      switch (newStatusCode) {
        case VoicePackServerStatusSuccessCode.NOT_INSTALLED:
          // Install the voice if it's not currently installed and it's marked
          // as a language that should be installed
          if (this.languagesForVoiceDownloads.has(lang)) {
            // Don't re-send install request if it's already been sent
            if (this.getVoicePackLocalStatus_(lang) !==
                VoiceClientSideStatusCode.SENT_INSTALL_REQUEST) {
              this.forceInstallRequest(lang, /* isRetry = */ false);
            }
          } else {
            this.setVoicePackLocalStatus(
                lang, VoiceClientSideStatusCode.NOT_INSTALLED);
          }
          break;
        case VoicePackServerStatusSuccessCode.INSTALLING:
          // Do nothing- we mark our local state as installing when we send the
          // request. Locally, we may time out a slow request and mark it as
          // errored, and we don't want to overwrite that state here.
          break;
        case VoicePackServerStatusSuccessCode.INSTALLED:
          // Force a refresh of the voices list since we might not get an update
          // the voices have changed.
          this.getVoices_(true);
          this.autoSwitchVoice_(lang);

          // Some languages may require a download from the voice pack
          // but may not have associated natural voices.
          const languageHasNaturalVoices = doesLanguageHaveNaturalVoices(lang);

          // Even though the voice may be installed on disk, it still may not be
          // available to the speechSynthesis API. Check whether to mark the
          // voice as AVAILABLE or INSTALLED_AND_UNAVAILABLE
          const voicesForLanguageAreAvailable = this.availableVoices_.some(
              voice =>
                  ((isNatural(voice) || !languageHasNaturalVoices) &&
                   getVoicePackConvertedLangIfExists(voice.lang) === lang));

          // If natural voices are currently available for the language or the
          // language does not support natural voices, set the status to
          // available. Otherwise, set the status to install and unavailabled.
          this.setVoicePackLocalStatus(
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
          this.setVoicePackLocalStatus(
              lang, VoiceClientSideStatusCode.ERROR_INSTALLING);
          break;
        case VoicePackServerStatusErrorCode.ALLOCATION:
          this.setVoicePackLocalStatus(
              lang, VoiceClientSideStatusCode.INSTALL_ERROR_ALLOCATION);
          break;
        default:
          // This ensures the switch statement is exhaustive
          return newStatusCode satisfies never;
      }
    } else {
      // Couldn't parse the response
      this.setVoicePackLocalStatus(
          lang, VoiceClientSideStatusCode.ERROR_INSTALLING);
    }
  }

  protected onLanguageMenuOpen_() {
    VoiceNotificationManager.getInstance().removeListener(this.$.languageToast);
  }

  protected onLanguageMenuClose_() {
    VoiceNotificationManager.getInstance().addListener(this.$.languageToast);
  }

  onVoicesChanged() {
    const previousSize = this.availableVoices_.length;
    // Get a new list of voices. This should be done before we call
    // refreshVoicePackStatuses();
    this.getVoices_(/*refresh =*/ true);

    if (this.shouldAttemptLanguageSettingsRestore && previousSize === 0 &&
        this.availableVoices_.length > 0) {
      // If we go from having no available voices to having voices available,
      // restore voice settings from preferences.
      this.restoreEnabledLanguagesFromPref();
      this.selectPreferredVoice();
    }

    // If voice was selected automatically and not by the user, check if
    // there's a higher quality voice available now.
    if (!this.currentVoiceIsUserChosen_()) {
      const naturalVoicesForLang = this.availableVoices_.filter(
          voice => isNatural(voice) &&
              voice.lang.startsWith(chrome.readingMode.baseLanguageForSpeech));

      if (naturalVoicesForLang) {
        this.selectedVoice_ = naturalVoicesForLang[0];
        this.resetSpeechPostSettingChange_();
      }
    }

    // Now that the voice list has changed, refresh the VoicePackStatuses in
    // case a language has been uninstalled.
    this.refreshVoicePackStatuses();

    // If the selected voice is now unavailable, such as after an uninstall,
    // reselect a new voice.
    if (this.selectedVoice_ &&
        !this.availableVoices_.some(
            voice => areVoicesEqual(voice, this.selectedVoice_!))) {
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
        v => this.enabledLangs.includes(v.lang.toLowerCase()));
    if (!voicesForCurrentEnabledLocale ||
        !voicesForCurrentEnabledLocale.length) {
      // If there's no enabled locales for this language, check for any other
      // voices for enabled locales.
      const allVoicesForEnabledLocales = allPossibleVoices.filter(
          v => this.enabledLangs.includes(v.lang.toLowerCase()));
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
  getAlternativeVoice(unavailableVoice: SpeechSynthesisVoice|
                      undefined): SpeechSynthesisVoice|undefined {
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

    // TODO(b/40927698): It's possible we can get stuck in an infinite loop
    // of jumping back and forth between two or more invalid voices, if
    // multiple voices are invalid. Investigate if we need to do more to handle
    // this case.

    // TODO(b/336596926): If there still aren't voices for the language,
    // attempt to fallback to the browser language, if we're using the page
    // language.
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

    // TODO(b/336596926): Handle language updates if there aren't any available
    // voices in the current language other than the unavailable voice.
    return undefined;
  }

  private getVoices_(refresh: boolean = false): SpeechSynthesisVoice[] {
    if (!this.availableVoices_.length || refresh) {
      this.availableVoices_ = getFilteredVoiceList(this.synth.getVoices());
      this.availableLangs_ =
          [...new Set(this.availableVoices_.map(({lang}) => lang))];

      this.populateDisplayNamesForLocaleCodes();
    }
    return this.availableVoices_;
  }

  private refreshVoicePackStatuses() {
    for (const lang of Object.keys(
             this.voicePackInstallStatusServerResponses_)) {
      this.sendGetVoicePackInfoRequest(lang);
    }
  }

  private getLangDisplayName(lang?: string): string {
    if (!lang) {
      return '';
    }
    const langLower = lang.toLowerCase();
    return this.localeToDisplayName_[langLower] || langLower;
  }

  private populateDisplayNamesForLocaleCodes() {
    this.localeToDisplayName_ = {};

    // Get display names for all the pack manager supported locales, only on
    // ChromeOS.
    if (chrome.readingMode.isChromeOsAsh) {
      AVAILABLE_GOOGLE_TTS_LOCALES.forEach((lang) => {
        this.maybeAddDisplayName(lang);
      });
    }

    // Get any remaining display names for languages of available voices.
    for (const {lang} of this.availableVoices_) {
      this.maybeAddDisplayName(lang);
    }
  }

  private maybeAddDisplayName(lang: string) {
    const langLower = lang.toLowerCase();
    if (!(langLower in this.localeToDisplayName_)) {
      const langDisplayName =
          chrome.readingMode.getDisplayNameForLocale(langLower, langLower);
      if (langDisplayName) {
        this.localeToDisplayName_ =
            {...this.localeToDisplayName_, [langLower]: langDisplayName};
      }
    }
  }

  private replaceElement(current: HTMLElement, replacer: Node) {
    const nodeId = this.domNodeToAxNodeIdMap_.get(current);
    assert(
        nodeId !== undefined,
        'trying to replace an element that doesn\'t exist');
    // Update map.
    this.domNodeToAxNodeIdMap_.delete(current);
    this.domNodeToAxNodeIdMap_.set(replacer, nodeId);
    // Replace element in DOM.
    current.replaceWith(replacer);
  }

  protected onPreviewVoice_(
      event: CustomEvent<{previewVoice: SpeechSynthesisVoice}>) {
    event.preventDefault();
    event.stopPropagation();

    this.stopSpeech(PauseActionSource.VOICE_PREVIEW);

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

    // TODO(b/40927698): There should probably be more sophisticated error
    // handling for voice previews, but for now, simply setting the preview
    // voice to null should be sufficient to reset state if an error is
    // encountered during a preview.
    utterance.onerror = () => {
      this.previewVoicePlaying_ = undefined;
    };

    this.synth.speak(utterance);
  }

  protected onVoiceMenuClose_(
      event: CustomEvent<{voicePlayingWhenMenuOpened: boolean}>) {
    event.preventDefault();
    event.stopPropagation();

    // TODO(b/323912186) Handle when menu is closed mid-preview and the user
    // presses play/pause button.
    if (!this.speechPlayingState.isSpeechActive &&
        event.detail.voicePlayingWhenMenuOpened) {
      this.playSpeech();
    }
  }

  protected onPlayPauseClick_() {
    if (this.speechPlayingState.isSpeechActive) {
      this.logSpeechPlaySession_();
      this.stopSpeech(PauseActionSource.BUTTON_CLICK);
    } else {
      this.playSessionStartTime = Date.now();
      this.playSpeech();
    }
  }

  stopSpeech(pauseSource: PauseActionSource) {
    this.speechPlayingState = {
      ...this.speechPlayingState,
      isSpeechActive: false,
      isAudioCurrentlyPlaying: false,
      pauseSource,
    };

    const pausedFromButton = pauseSource === PauseActionSource.BUTTON_CLICK;

    // Voice and speed changes take effect on the next call of synth.play(),
    // but not on .resume(). In order to be responsive to the user's settings
    // changes, we call synth.cancel() and synth.play(). However, we don't do
    // synth.cancel() and synth.play() when user clicks play/pause button,
    // because synth.cancel() and synth.play() plays from the beginning of the
    // current utterance, even if parts of it had been spoken already.
    // Therefore, when a user toggles the play/pause button, we call
    // synth.pause() and synth.resume() for speech to resume from where it left
    // off.
    if (pausedFromButton) {
      this.synth.pause();
    } else {
      // Canceling clears all the Utterances that are queued up via synth.play()
      this.synth.cancel();
    }

    // Restore links if they're enabled when speech pauses. Don't restore links
    // if it's paused from a non-pause button (e.g. voice previews) so the links
    // don't flash off and on.
    if (chrome.readingMode.linksEnabled && pausedFromButton) {
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
    this.synth.cancel();
    this.resetPreviousHighlight_();
    // Reset the word boundary index whenever we move the granularity position.
    this.resetToDefaultWordBoundaryState();
    chrome.readingMode.movePositionToNextGranularity();

    if (!this.highlightAndPlayMessage()) {
      this.onSpeechFinished();
    }
  }

  protected playPreviousGranularity_() {
    this.synth.cancel();
    // This must be called BEFORE calling
    // chrome.readingMode.movePositionToPreviousGranularity so we can accurately
    // determine what's currently being highlighted.
    this.resetPreviousHighlightAndRemoveCurrentHighlight();
    // Reset the word boundary index whenever we move the granularity position.
    this.resetToDefaultWordBoundaryState();
    chrome.readingMode.movePositionToPreviousGranularity();

    if (!this.highlightAndPlayMessage()) {
      this.onSpeechFinished();
    }
  }

  playSpeech() {
    const container = this.$.container;
    const {anchorNode, anchorOffset, focusNode, focusOffset} =
        this.getSelection();
    const hasSelection =
        anchorNode !== focusNode || anchorOffset !== focusOffset;
    if (this.speechPlayingState.hasSpeechBeenTriggered &&
        !this.speechPlayingState.isSpeechActive) {
      const pausedFromButton = this.speechPlayingState.pauseSource ===
          PauseActionSource.BUTTON_CLICK;

      let playedFromSelection = false;
      if (hasSelection) {
        this.synth.cancel();
        this.resetToDefaultWordBoundaryState();
        playedFromSelection = this.playFromSelection();
      }

      if (!playedFromSelection) {
        if (pausedFromButton &&
            this.wordBoundaryState.mode !==
                WordBoundaryMode.BOUNDARY_DETECTED) {
          // If word boundaries aren't supported for the given voice, we should
          // still continue to use synth.resume, as this is preferable to
          // restarting the current message.
          this.synth.resume();
        } else {
          this.synth.cancel();
          if (!this.highlightAndPlayInterruptedMessage()) {
            // Ensure we're updating Read Aloud state if there's no text to
            // speak.
            this.onSpeechFinished();
          }
        }
      }

      this.speechPlayingState = {
        isSpeechTreeInitialized:
            this.speechPlayingState.isSpeechTreeInitialized,
        isSpeechActive: true,
        isAudioCurrentlyPlaying:
            this.speechPlayingState.isAudioCurrentlyPlaying,
        hasSpeechBeenTriggered: this.speechPlayingState.hasSpeechBeenTriggered,
      };

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
      if (!playedFromSelection &&
          !container.querySelector('.' + currentReadHighlightClass)) {
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
      this.speechPlayingState = {
        isSpeechTreeInitialized:
            this.speechPlayingState.isSpeechTreeInitialized,
        isSpeechActive: true,
        isAudioCurrentlyPlaying:
            this.speechPlayingState.isAudioCurrentlyPlaying,
        hasSpeechBeenTriggered: true,
      };
      // Hide links when speech begins playing.
      if (chrome.readingMode.linksEnabled) {
        this.updateLinks_();
      }

      const playedFromSelection = hasSelection && this.playFromSelection();
      if (!playedFromSelection && this.firstTextNodeSetForReadAloud) {
        if (!this.speechPlayingState.isSpeechTreeInitialized) {
          this.initializeSpeechTree();
        }
        if (!this.highlightAndPlayMessage()) {
          // Ensure we're updating Read Aloud state if there's no text to speak.
          this.onSpeechFinished();
        }
      }
    }
  }

  initializeSpeechTree() {
    if (this.firstTextNodeSetForReadAloud) {
      // TODO(crbug.com/40927698): There should be a way to use AXPosition so
      // that this step can be skipped.
      chrome.readingMode.initAxPositionWithNode(
          this.firstTextNodeSetForReadAloud);
      this.speechPlayingState = {
        isAudioCurrentlyPlaying:
            this.speechPlayingState.isAudioCurrentlyPlaying,
        isSpeechActive: this.speechPlayingState.isSpeechActive,
        isSpeechTreeInitialized: true,
        hasSpeechBeenTriggered: this.speechPlayingState.hasSpeechBeenTriggered,
      };

      this.preprocessTextForSpeech();
    }
  }

  async preprocessTextForSpeech() {
    chrome.readingMode.preprocessTextForSpeech();
  }

  private getSelectedIds(): {
    anchorNodeId: number|undefined,
    anchorOffset: number,
    focusNodeId: number|undefined,
    focusOffset: number,
  } {
    const {anchorNode, anchorOffset, focusNode, focusOffset} =
        this.getSelection();
    let anchorNodeId = this.domNodeToAxNodeIdMap_.get(anchorNode);
    let focusNodeId = this.domNodeToAxNodeIdMap_.get(focusNode);
    let adjustedAnchorOffset = anchorOffset;
    let adjustedFocusOffset = focusOffset;
    if (!anchorNodeId) {
      anchorNodeId = this.getHighlightedAncestorId_(anchorNode);
      adjustedAnchorOffset += this.getOffsetInAncestor(anchorNode);
    }
    if (!focusNodeId) {
      focusNodeId = this.getHighlightedAncestorId_(focusNode);
      adjustedFocusOffset += this.getOffsetInAncestor(focusNode);
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

    const {anchorNodeId, anchorOffset, focusNodeId, focusOffset} =
        this.getSelectedIds();
    // If only one of the ids is present, use that one.
    let startingNodeId: number|undefined =
        anchorNodeId ? anchorNodeId : focusNodeId;
    let startingOffset = anchorNodeId ? anchorOffset : focusOffset;
    // If both are present, start with the node that is sooner in the page.
    if (anchorNodeId && focusNodeId) {
      const pos =
          selection.anchorNode.compareDocumentPosition(selection.focusNode);
      const focusIsFirst = pos === Node.DOCUMENT_POSITION_PRECEDING;
      startingNodeId = focusIsFirst ? focusNodeId : anchorNodeId;
      startingOffset = focusIsFirst ? focusOffset : anchorOffset;
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
      this.resetPreviousHighlight_();
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
          currentTextIds, /*scrollIntoView=*/ false);
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
  // TODO (crbug.com/1474951): Investigate using AXRange.GetText to get text
  // between start node / end nodes and their offsets.
  highlightAndPlayMessage(isInterrupted: boolean = false): boolean {
    // getCurrentText gets the AX Node IDs of text that should be spoken and
    // highlighted.
    const axNodeIds: number[] = chrome.readingMode.getCurrentText();

    // If there aren't any valid ax node ids returned by getCurrentText,
    // speech should stop.
    if (axNodeIds.length === 0) {
      return false;
    }

    const utteranceText = this.extractTextOf(axNodeIds);
    // If node ids were returned but they don't exist in the Reading Mode panel,
    // there's been a mismatch between Reading Mode and Read Aloud. In this
    // case, we should move to the next Read Aloud node and attempt to continue
    // playing.
    if (!utteranceText) {
      // TODO(b/332694565): This fallback should never be needed, but it is.
      // Investigate root cause of Read Aloud / Reading Mode mismatch.
      chrome.readingMode.movePositionToNextGranularity();
      return this.highlightAndPlayMessage(isInterrupted);
    }

    // The TTS engine may not like attempts to speak whitespace, so move to the
    // next utterance.
    if (utteranceText.trim().length === 0) {
      chrome.readingMode.movePositionToNextGranularity();
      return this.highlightAndPlayMessage(isInterrupted);
    }

    // If we're resuming a previously interrupted message, use word
    // boundaries (if available) to resume at the beginning of the current
    // word.
    if (isInterrupted &&
        this.wordBoundaryState.mode === WordBoundaryMode.BOUNDARY_DETECTED) {
      const substringIndex = this.wordBoundaryState.previouslySpokenIndex +
          this.wordBoundaryState.speechUtteranceStartIndex;
      this.wordBoundaryState.previouslySpokenIndex = 0;
      this.wordBoundaryState.speechUtteranceStartIndex = substringIndex;
      const utteranceTextForWordBoundary =
          utteranceText.substring(substringIndex);
      // Don't use the word boundary if it's going to cause a TTS engine issue.
      if (utteranceTextForWordBoundary.trim().length === 0) {
        this.playText(utteranceText);
      } else {
        this.playText(utteranceText.substring(substringIndex));
      }
    } else {
      this.playText(utteranceText);
    }

    this.highlightCurrentGranularity(axNodeIds);
    return true;
  }

  // Highlights or rehighlights the current granularity, sentence or word.
  highlightCurrentGranularity(
      axNodeIds: number[], scrollIntoView: boolean = true) {
    const highlightGranularity = this.getEffectiveHighlightingGranularity_();
    switch (highlightGranularity) {
      case chrome.readingMode.noHighlighting:
      // Even without highlighting, we still calculate the sentence highlight,
      // so that it's visible as soon as the user turns on sentence
      // highlighting. The highlight will not be visible, since the highlight
      // color in this case will be transparent.
      case chrome.readingMode.sentenceHighlighting:
        this.highlightCurrentSentence(axNodeIds, scrollIntoView);
        break;
      case chrome.readingMode.wordHighlighting:
        this.highlightCurrentWord();
        break;
      case chrome.readingMode.phraseHighlighting:
        this.highlightCurrentPhrase();
        break;
      case chrome.readingMode.autoHighlighting:
      default:
        // This cannot happen, but ensures the switch statement is exhaustive.
        assert(false, 'invalid value for effective highlight');
    }
  }

  // Gets the accessible text boundary for the given string.
  getAccessibleTextLength(utteranceText: string): number {
    // Splicing on commas won't work for all locales, but since this is a
    // simple strategy for splicing text in languages that do use commas
    // that reduces the need for calling getAccessibleBoundary.
    // TODO(crub.com/1474951): Investigate if we can utilize comma splices
    // directly in the utils methods called by #getAccessibleBoundary.
    const lastCommaIndex =
        utteranceText.substring(0, this.maxSpeechLength).lastIndexOf(',');

    // To prevent infinite looping, only use the lastCommaIndex if it's not the
    // first character. Otherwise, use getAccessibleBoundary to prevent
    // repeatedly splicing on the first comma of the same substring.
    if (lastCommaIndex > 0) {
      return lastCommaIndex;
    }

    // TODO(crbug.com/40927698): getAccessibleBoundary breaks on the nearest
    // word boundary, but if there's some type of punctuation (such as a comma),
    // it would be preferable to break on the punctuation so the pause in
    // speech sounds more natural.
    return chrome.readingMode.getAccessibleBoundary(
        utteranceText, this.maxSpeechLength);
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
        utteranceText.length > this.maxSpeechLength;
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
      // We can't be sure that the engine has loaded at this point, but
      // if there's an error, we want to ensure we keep the play buttons
      // to prevent trapping users in a state where they can no longer play
      // Read Aloud, as this is preferable to a long delay before speech
      // with no feedback.
      this.speechEngineLoaded_ = true;

      if (error.error === 'interrupted') {
        // SpeechSynthesis.cancel() was called, therefore, do nothing.
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
        this.synth.cancel();
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
      }

      // No appropriate voice is available for the language designated in
      // SpeechSynthesisUtterance lang.
      if (error.error === 'language-unavailable') {
        const possibleNewLanguage = convertLangToAnAvailableLangIfPresent(
            this.speechSynthesisLanguage, this.availableLangs_,
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
      // TODO(b/40927698: Consider showing an error message.
      this.stopSpeech(PauseActionSource.DEFAULT);
    };

    message.addEventListener('boundary', (event) => {
      // Some voices may give sentence boundaries, but we're only concerned
      // with word boundaries in boundary event because we're speaking text at
      // the sentence granularity level, so we'll retrieve these boundaries in
      // message.onEnd instead.
      if (event.name === 'word') {
        this.updateBoundary(event.charIndex);

        const highlightGranularity =
            this.getEffectiveHighlightingGranularity_();
        switch (highlightGranularity) {
          case chrome.readingMode.noHighlighting:
          case chrome.readingMode.sentenceHighlighting:
            // No need to update the highlight on word boundary events if
            // highlighting is off or if sentence highlighting is used.
            break;
          case chrome.readingMode.wordHighlighting:
            this.highlightCurrentWord();
            break;
          case chrome.readingMode.phraseHighlighting:
            this.highlightCurrentPhrase();
            break;
          case chrome.readingMode.autoHighlighting:
          default:
            // This cannot happen, but ensures the switch statement is
            // exhaustive.
            assert(false, 'invalid value for effective highlight');
        }
      }
    });

    message.onstart = () => {
      // We've gotten the signal that the speech engine has loaded, therefore
      // we can enable the Read Aloud buttons.
      this.speechEngineLoaded_ = true;

      if (!this.speechPlayingState.isAudioCurrentlyPlaying) {
        this.speechPlayingState = {
          ...this.speechPlayingState,
          isAudioCurrentlyPlaying: true,
        };
      }
    };

    message.onend = () => {
      if (isTextTooLong) {
        // Since our previous utterance was too long, continue speaking pieces
        // of the current utterance until the utterance is complete. The entire
        // utterance is highlighted, so there's no need to update highlighting
        // until the utterance substring is an acceptable size.
        this.playText(utteranceText.substring(endBoundary));
        return;
      }

      // Now that we've finiished reading this utterance, update the Granularity
      // state to point to the next one
      // Reset the word boundary index whenever we move the granularity
      // position.
      this.resetToDefaultWordBoundaryState();
      chrome.readingMode.movePositionToNextGranularity();
      // Continue speaking with the next block of text.
      if (!this.highlightAndPlayMessage()) {
        this.onSpeechFinished();
      }
    };

    const voice = this.getSpeechSynthesisVoice();
    if (!voice) {
      // TODO(crbug.com/40927698): Handle when no voices are available.
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
    this.synth.speak(message);
  }

  updateBoundary(charIndex: number) {
    this.wordBoundaryState.previouslySpokenIndex = charIndex;
    this.wordBoundaryState.mode = WordBoundaryMode.BOUNDARY_DETECTED;
  }

  resetToDefaultWordBoundaryState(
      possibleWordBoundarySupportChange: boolean = false) {
    this.wordBoundaryState = {
      previouslySpokenIndex: 0,
      // If a boundary has been detected, the mode should be reset to
      // NO_BOUNDARIES instead of BOUNDARIES_NOT_SUPPORTED because we know word
      // boundaries are supported- we just need to clear the current boundary
      // state. This allows us to highlight the next word at the start of a
      // sentence when playback state changes.
      // However, if there's been a change that potentially impacts if word
      // boundaries are supported (such as changing the voice), we should
      // reset to BOUNDARIES_NOT_SUPPORTED because we don't know yet if word
      // boundaries are supported for this voice.
      mode: ((this.wordBoundaryState.mode ===
              WordBoundaryMode.BOUNDARY_DETECTED) &&
             !possibleWordBoundarySupportChange) ?
          WordBoundaryMode.NO_BOUNDARIES :
          WordBoundaryMode.BOUNDARIES_NOT_SUPPORTED,
      speechUtteranceStartIndex: 0,
    };
  }

  private extractTextOf(axNodeIds: number[]): string {
    let utteranceText: string = '';
    for (let i = 0; i < axNodeIds.length; i++) {
      assert(axNodeIds[i], 'trying to get text from an undefined node id');
      const nodeId = axNodeIds[i];
      const startIndex = chrome.readingMode.getCurrentTextStartIndex(nodeId);
      const endIndex = chrome.readingMode.getCurrentTextEndIndex(nodeId);
      const element = this.domNodeToAxNodeIdMap_.keyFrom(nodeId);
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

  highlightCurrentWord() {
    this.highlightCurrentWordOrPhrase_(false);
  }

  highlightCurrentPhrase() {
    this.highlightCurrentWordOrPhrase_(true);
  }

  // TODO(b/301131238): Verify all edge cases.
  private highlightCurrentWordOrPhrase_(highlightPhrases: boolean) {
    // Word highlights can be called quite frequently which can create some
    // misordering, so just make sure we've cleared the prior current word
    // highlight before showing the next one.
    this.resetCurrentHighlight();
    this.resetPreviousHighlight_();
    const index = this.wordBoundaryState.speechUtteranceStartIndex +
        this.wordBoundaryState.previouslySpokenIndex;
    const highlightNodes =
        chrome.readingMode.getHighlightForCurrentSegmentIndex(
            index, highlightPhrases);
    let anyHighlighted: boolean = false;
    for (let i = 0; i < highlightNodes.length; i++) {
      const highlightNode = highlightNodes[i].nodeId;
      const highlightLength: number = highlightNodes[i].length;
      const highlightStartIndex = highlightNodes[i].start;
      const endIndex = highlightStartIndex + highlightLength;
      const element = this.domNodeToAxNodeIdMap_.keyFrom(highlightNode);
      if (!element ||
          isInvalidHighlightForWordHighlighting(
              element.textContent?.substring(highlightStartIndex, endIndex)
                  .trim())) {
        continue;
      }
      anyHighlighted = true;
      this.highlightCurrentText_(
          highlightStartIndex, endIndex, element as HTMLElement);
    }
    if (anyHighlighted) {
      // Only scroll if at least one node was highlighted.
      this.scrollHighlightIntoView();
    }
  }

  highlightCurrentSentence(
      nextTextIds: number[], scrollIntoView: boolean = true) {
    if (nextTextIds.length === 0) {
      return;
    }

    this.resetPreviousHighlight_();
    for (let i = 0; i < nextTextIds.length; i++) {
      const nodeId = nextTextIds[i];
      const element = this.domNodeToAxNodeIdMap_.keyFrom(nodeId) as HTMLElement;
      if (!element) {
        continue;
      }
      const start = chrome.readingMode.getCurrentTextStartIndex(nodeId);
      const end = chrome.readingMode.getCurrentTextEndIndex(nodeId);
      if ((start < 0) || (end < 0)) {
        // If the start or end index is invalid, don't use this node.
        continue;
      }
      this.highlightCurrentText_(start, end, element);
    }

    if (!scrollIntoView) {
      return;
    }

    this.scrollHighlightIntoView();
  }

  private scrollHighlightIntoView() {
    // Ensure all the current highlights are in view.
    // TODO: b/40927698 - Handle if the highlight is longer than the full height
    // of the window (e.g. when font size is very large). Possibly using word
    // boundaries to know when we've reached the bottom of the window and need
    // to scroll so the rest of the current highlight is showing.
    assert(this.shadowRoot);
    const currentHighlights = this.shadowRoot!.querySelectorAll<HTMLElement>(
        '.' + currentReadHighlightClass);
    if (!currentHighlights) {
      return;
    }
    const firstHighlight = currentHighlights.item(0);
    const lastHighlight = currentHighlights.item(currentHighlights.length - 1);
    const highlightBottom = lastHighlight.getBoundingClientRect().bottom;
    const highlightTop = firstHighlight.getBoundingClientRect().top;
    const highlightHeight = highlightBottom - highlightTop;
    if (highlightHeight > (window.innerHeight / 2)) {
      // If the bottom of the highlight would be offscreen if we center it,
      // scroll the first highlight to the top instead of centering it.
      firstHighlight.scrollIntoView({block: 'start'});
    } else if ((highlightBottom > window.innerHeight) || (highlightTop < 0)) {
      // Otherwise center the current highlight if part of it would be cut off.
      firstHighlight.scrollIntoView({block: 'center'});
    }
  }

  private defaultUtteranceSettings(): UtteranceSettings {
    const lang = this.speechSynthesisLanguage;

    return {
      lang,
      // TODO(crbug.com/40927698): Ensure the rate is valid for the current
      // speech engine.
      rate: getCurrentSpeechRate(),
      volume: 1,
      pitch: 1,
    };
  }

  // The following results in
  // <span>
  //   <span class="previous-read-highlight"> prefix text </span>
  //   <span class="current-read-highlight"> highlighted text </span>
  //   suffix text
  // </span>
  private highlightCurrentText_(
      highlightStart: number, highlightEnd: number,
      currentNode: HTMLElement): void {
    const parentOfHighlight = document.createElement('span');
    parentOfHighlight.classList.add(parentOfHighlightClass);

    // First pull out any text within this node before the highlighted section.
    // Since it's already been highlighted, we fade it out.
    const highlightPrefix =
        currentNode.textContent!.substring(0, highlightStart);
    if (highlightPrefix.length > 0) {
      const prefixNode = document.createElement('span');
      prefixNode.classList.add(previousReadHighlightClass);
      prefixNode.textContent = highlightPrefix;
      this.previousHighlights_.push(prefixNode);
      parentOfHighlight.appendChild(prefixNode);
    }

    // Then get the section of text to highlight and mark it for
    // highlighting.
    const readingHighlight = document.createElement('span');
    readingHighlight.classList.add(currentReadHighlightClass);
    const textNode = document.createTextNode(
        currentNode.textContent!.substring(highlightStart, highlightEnd));
    readingHighlight.appendChild(textNode);
    this.highlightedNodeToOffsetInParent.set(textNode, highlightStart);
    parentOfHighlight.appendChild(readingHighlight);

    // Finally, append the rest of the text for this node that has yet to be
    // highlighted.
    const highlightSuffix = currentNode.textContent!.substring(highlightEnd);
    if (highlightSuffix.length > 0) {
      const suffixNode = document.createTextNode(highlightSuffix);
      this.highlightedNodeToOffsetInParent.set(suffixNode, highlightEnd);
      parentOfHighlight.appendChild(suffixNode);
    }

    // Replace the current node in the tree with the split up version of the
    // node.
    this.previousHighlights_.push(readingHighlight);
    this.replaceElement(currentNode, parentOfHighlight);
  }

  private onSpeechFinished() {
    this.clearReadAloudState();

    // Show links when speech finishes playing.
    if (chrome.readingMode.linksEnabled) {
      this.updateLinks_();
    }
    // Clear the formatting we added for highlighting.
    this.updateContent();
    this.logSpeechPlaySession_();
  }

  private clearReadAloudState() {
    this.speechPlayingState = {
      isSpeechActive: false,
      pauseSource: PauseActionSource.DEFAULT,
      isSpeechTreeInitialized: false,
      isAudioCurrentlyPlaying: false,
      hasSpeechBeenTriggered: false,
    };
    this.previousHighlights_ = [];
    this.resetToDefaultWordBoundaryState();
  }

  private getEffectiveHighlightingGranularity_(): number {
    // Parse all of the conditions that control highlighting and return the
    // effective highlighting granularity.
    const highlight = chrome.readingMode.highlightGranularity;

    if (highlight === chrome.readingMode.noHighlighting ||
        highlight === chrome.readingMode.sentenceHighlighting) {
      return highlight;
    }

    if (!chrome.readingMode.isAutomaticWordHighlightingEnabled ||
        this.wordBoundaryState.mode ===
            WordBoundaryMode.BOUNDARIES_NOT_SUPPORTED ||
        isEspeak(this.selectedVoice_)) {
      // Fall back where word highlighting is not possible. Since espeak
      // boundaries are different than Google TTS word boundaries, fall back to
      // sentence boundaries in that case too.
      return chrome.readingMode.sentenceHighlighting;
    }

    const currentSpeechRate: number = getCurrentSpeechRate();

    if (!chrome.readingMode.isPhraseHighlightingEnabled) {
      // Choose sentence highlighting for fast voices.
      if (currentSpeechRate > 1.2 &&
          highlight === chrome.readingMode.autoHighlighting) {
        return chrome.readingMode.sentenceHighlighting;
      }

      // In other cases where phrase highilghting is off, choose word
      // highlighting.
      return chrome.readingMode.wordHighlighting;
    }

    // TODO(crbug.com/364327601): Check that the language of the page should
    // be English for phrase highlighting.
    if (highlight === chrome.readingMode.autoHighlighting) {
      if (currentSpeechRate <= 0.8) {
        return chrome.readingMode.wordHighlighting;
      } else if (currentSpeechRate >= 2.0) {
        return chrome.readingMode.sentenceHighlighting;
      } else {
        return chrome.readingMode.phraseHighlighting;
      }
    }

    // In other cases, return what the user selected (i.e. word/phrase).
    return highlight;
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
      this.resetToDefaultWordBoundaryState(
          /*possibleWordBoundarySupportChange=*/ true);
    }

    this.resetSpeechPostSettingChange_();
  }

  protected onVoiceLanguageToggle_(event: CustomEvent<{language: string}>) {
    event.preventDefault();
    event.stopPropagation();
    const toggledLanguage = event.detail.language;
    const currentlyEnabled = this.enabledLangs.includes(toggledLanguage);

    if (!currentlyEnabled) {
      this.autoSwitchVoice_(toggledLanguage);
      this.installVoicePackIfPossible(
          toggledLanguage, /* onlyInstallExactGoogleLocaleMatch=*/ true,
          /* retryIfPreviousInstallFailed= */ true);
    } else {
      // If the language has been deselected, remove the language from the list
      // of language packs to download
      const langCodeForVoicePackManager =
          convertLangOrLocaleForVoicePackManager(toggledLanguage);
      if (langCodeForVoicePackManager) {
        this.languagesForVoiceDownloads.delete(langCodeForVoicePackManager);
      }
    }
    this.enabledLangs = currentlyEnabled ?
        this.enabledLangs.filter(lang => lang !== toggledLanguage) :
        [...this.enabledLangs, toggledLanguage];

    chrome.readingMode.onLanguagePrefChange(toggledLanguage, !currentlyEnabled);

    if (!currentlyEnabled && !this.selectedVoice_) {
      // If there were no enabled languages (and thus no selected voice), select
      // a voice.
      this.getSpeechSynthesisVoice();
    }
  }

  protected resetSpeechPostSettingChange_() {
    // Don't call stopSpeech() if the speech tree hasn't been initialized or
    // if speech hasn't been triggered yet.
    if (!this.speechPlayingState.isSpeechTreeInitialized ||
        !this.speechPlayingState.hasSpeechBeenTriggered) {
      return;
    }

    const playSpeechOnChange = this.speechPlayingState.isSpeechActive;

    // Cancel the queued up Utterance using the old speech settings
    this.stopSpeech(PauseActionSource.VOICE_SETTINGS_CHANGE);

    // If speech was playing when a setting was changed, continue playing speech
    if (playSpeechOnChange) {
      this.playSpeech();
    }
  }

  // This must be called BEFORE calling
  // chrome.readingMode.movePositionToPreviousGranularity so we can accurately
  // determine what's currently being highlighted.
  private resetPreviousHighlightAndRemoveCurrentHighlight() {
    this.removeCurrentHighlight();
    this.resetPreviousHighlight_();
  }

  // Resets formatting on the current highlight, including previous highlight
  // formatting.
  private removeCurrentHighlight() {
    // The most recent highlight could have been spread across multiple segments
    // so clear the formatting for all of the segments.
    for (let i = 0; i < chrome.readingMode.getCurrentText().length; i++) {
      const lastElement = this.previousHighlights_.pop();
      if (lastElement) {
        lastElement.classList.remove(currentReadHighlightClass);
      }
    }
  }

  // Resets the current highlight. Does not change how this element will
  // be considered for previous highlighting.
  private resetCurrentHighlight() {
    const elements =
        this.shadowRoot?.querySelectorAll('.' + currentReadHighlightClass);
    elements?.forEach(element => {
      element.classList.remove(currentReadHighlightClass);
    });
  }

  private resetPreviousHighlight_() {
    this.previousHighlights_.forEach((element) => {
      if (element) {
        element.classList.add(previousReadHighlightClass);
        element.classList.remove(currentReadHighlightClass);
      }
    });
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
    // TODO(crbug.com/40927698): Remove this call. Using this.settingsPrefs_
    // should replace this direct call to the toolbar.
    this.$.toolbar.restoreSettingsFromPrefs();
  }

  restoreEnabledLanguagesFromPref() {
    // We need to make sure the languages we choose correspond to voices, so
    // refresh the list of voices and available langs
    this.getVoices_();

    // If there are no available languages or voices yet, we might not be
    // able to restore voice settings yet, so signal that we should attempt
    // to restore settings the next time onVoicesChanged is called with
    // available voices.
    this.shouldAttemptLanguageSettingsRestore =
        !(this.availableLangs_ && this.availableLangs_.length > 0);

    const storedLanguagesPref: string[] =
        chrome.readingMode.getLanguagesEnabledInPref();
    const browserOrPageBaseLang = chrome.readingMode.baseLanguageForSpeech;
    this.speechSynthesisLanguage = browserOrPageBaseLang;

    this.enabledLangs = createInitialListOfEnabledLanguages(
        browserOrPageBaseLang, storedLanguagesPref, this.availableLangs_,
        this.defaultVoice()?.lang);

    storedLanguagesPref.forEach(storedLanguage => {
      if (!this.enabledLangs.find(language => language === storedLanguage)) {
        // If a stored language doesn't have a match in the enabled languages
        // list, disable the original preference. This can guard against issues
        // with preferences after bugs are fixed.
        // e.g. if "de-DE" is accidentally stored as a language, the preference
        // will always be converted to "de-de" in
        // #createInitialListOfEnabledLanguages, and if we disable the
        // preference, "de-de" will be disabled, meaning the original
        // pref will never be deleted and it will be impossible to disable
        // the preference.
        chrome.readingMode.onLanguagePrefChange(storedLanguage, false);
      }
    });

    for (const lang of this.enabledLangs) {
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
    // TODO: b/40275871 - decide whether this is the behavior we want. This
    // shouldn't happen often, so just skip selecting a new voice for now.
    // Another option would be to update the voice and the call
    // resetSpeechPostSettingsChange(), but that could be jarring.
    if (this.speechPlayingState.hasSpeechBeenTriggered) {
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
    if (this.selectedVoice_ &&
        !this.enabledLangs.includes(this.selectedVoice_.lang)) {
      this.enabledLangs = [...this.enabledLangs, this.selectedVoice_.lang];
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

    // TODO(crbug.com/366002886): Re-highlight with the new granularity. In
    // particular, when switching from word or phrase to sentence, the sentence
    // highlight needs to be recalculated.

    // TODO(crbug.com/364546547): Log these highlight granularity changes when
    // the phrase menu is shown. (Toggles are already logged in the toolbar.)
  }

  // If the screen is locked during speech, we should stop speaking.
  onLockScreen() {
    if (this.speechPlayingState.isSpeechActive) {
      this.stopSpeech(PauseActionSource.DEFAULT);
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
    if (!chrome.readingMode.isAutoVoiceSwitchingEnabled) {
      return;
    }

    // Only enable this language if it has available voices and is the current
    // language. Otherwise switch to a default voice if nothing is selected.
    const availableLang =
        convertLangToAnAvailableLangIfPresent(lang, this.availableLangs_);
    if (!availableLang ||
        !availableLang.startsWith(this.speechSynthesisLanguage.split('-')[0])) {
      this.selectPreferredVoice();
      return;
    }

    // Only enable Google TTS supported locales for this language if they exist.
    let localesToEnable: string[] = [];
    const voicePackLocale =
        convertLangOrLocaleToExactVoicePackLocale(availableLang);
    if (voicePackLocale) {
      localesToEnable.push(voicePackLocale);
    } else {
      // If there are no Google TTS locales for this language then enable any
      // available locale for this language.
      localesToEnable =
          this.availableLangs_.filter(l => l.startsWith(availableLang));
    }

    // Enable the locales so we can select a voice for the given language and
    // show it in the voice menu.
    localesToEnable.forEach(langToEnable => {
      if (!this.enabledLangs.includes(langToEnable)) {
        this.enabledLangs = [...this.enabledLangs, langToEnable];
      }
    });
    this.selectPreferredVoice();
  }

  // Kicks off a workflow to install a voice pack.
  // 1) Checks if Language Pack Manager supports a version of this voice/locale
  // 2) If so, adds voice to installVoicePackIfPossible set
  // 3) Kicks off request GetVoicePackInfo to see if the voice is installed
  // 4) Upon response, if we see the voice is not installed and that it's in
  // installVoicePackIfPossible, then we trigger an install request
  private installVoicePackIfPossible(
      langOrLocale: string, onlyInstallExactGoogleLocaleMatch: boolean,
      retryIfPreviousInstallFailed: boolean) {
    if (!chrome.readingMode.isLanguagePackDownloadingEnabled) {
      return;
    }

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
        langOrLocale, this.enabledLangs, this.availableLangs_);

    if (!langCodeForVoicePackManager) {
      this.autoSwitchVoice_(langOrLocale);
      return;
    }

    const statusForLang =
        this.voicePackInstallStatusServerResponses_[langCodeForVoicePackManager];

    if (!statusForLang) {
      if (retryIfPreviousInstallFailed) {
        this.forceInstallRequest(
            langCodeForVoicePackManager, /* isRetry = */ false);
      } else {
        this.languagesForVoiceDownloads.add(langCodeForVoicePackManager);
        // Inquire if the voice pack is downloaded. If not, it'll trigger a
        // download when we get the response in updateVoicePackStatus().
        this.sendGetVoicePackInfoRequest(langCodeForVoicePackManager);
      }
      return;
    }

    // If we send an install request for this language, we'll auto switch
    // voices after it installs.
    if (isVoicePackStatusSuccess(statusForLang) &&
        statusForLang.code === VoicePackServerStatusSuccessCode.NOT_INSTALLED) {
      this.languagesForVoiceDownloads.add(langCodeForVoicePackManager);
      // Inquire if the voice pack is downloaded. If not, it'll trigger a
      // download when we get the response in updateVoicePackStatus().
      this.sendGetVoicePackInfoRequest(langCodeForVoicePackManager);
    } else if (
        retryIfPreviousInstallFailed && isVoicePackStatusError(statusForLang)) {
      this.languagesForVoiceDownloads.add(langCodeForVoicePackManager);

      // If the previous install attempt failed (e.g. due to no internet
      // connection), the PackManager sends a failure for subsequent GetInfo
      // requests. Therefore, we need to bypass our normal flow of calling
      // GetInfo to see if the voice is available to install, and just call
      // sendInstallVoicePackRequest directly
      this.forceInstallRequest(
          langCodeForVoicePackManager, /* isRetry = */ true);
    } else {
      this.autoSwitchVoice_(langCodeForVoicePackManager);
    }
  }

  private forceInstallRequest(
      langCodeForVoicePackManager: string, isRetry: boolean) {
    this.setVoicePackLocalStatus(
        langCodeForVoicePackManager,
        isRetry ? VoiceClientSideStatusCode.SENT_INSTALL_REQUEST_ERROR_RETRY :
                  VoiceClientSideStatusCode.SENT_INSTALL_REQUEST);

    chrome.readingMode.sendInstallVoicePackRequest(langCodeForVoicePackManager);
  }

  protected onKeyDown_(e: KeyboardEvent) {
    if (e.key === 'k') {
      e.stopPropagation();
      this.onPlayPauseClick_();
    }
  }

  getVoicePackStatusForTesting(lang: string):
      {server: VoicePackStatus, client: VoiceClientSideStatusCode} {
    const server = this.getVoicePackServerStatus_(lang);
    const client = this.getVoicePackLocalStatus_(lang);
    assert(server);
    assert(client);
    return {server, client};
  }

  private getVoicePackServerStatus_(lang: string): VoicePackStatus|undefined {
    const voicePackLanguage = getVoicePackConvertedLangIfExists(lang);
    return this.voicePackInstallStatusServerResponses_[voicePackLanguage];
  }

  private getVoicePackLocalStatus_(lang: string): VoiceClientSideStatusCode
      |undefined {
    const voicePackLanguage = getVoicePackConvertedLangIfExists(lang);
    return this.voiceStatusLocalState_[voicePackLanguage];
  }

  setVoicePackLocalStatus(lang: string, status: VoiceClientSideStatusCode) {
    const possibleVoicePackLanguage =
        convertLangOrLocaleForVoicePackManager(lang);
    const voicePackLanguage =
        possibleVoicePackLanguage ? possibleVoicePackLanguage : lang;
    const oldStatus = this.voiceStatusLocalState_[voicePackLanguage];
    this.voiceStatusLocalState_ = {
      ...this.voiceStatusLocalState_,
      [voicePackLanguage]: status,
    };

    // No need for notifications for non-Google TTS languages.
    if ((possibleVoicePackLanguage !== undefined) && (oldStatus !== status)) {
      this.notificationManager_.onVoiceStatusChange(
          voicePackLanguage, status, this.availableVoices_);
    }
  }

  resetVoiceForTesting() {
    this.selectedVoice_ = undefined;
  }

  private setVoicePackServerStatus_(lang: string, status: VoicePackStatus) {
    // Convert the language string to ensure consistency across
    // languages and locales when setting the status.
    const voicePackLanguage = getVoicePackConvertedLangIfExists(lang);
    this.voicePackInstallStatusServerResponses_ = {
      ...this.voicePackInstallStatusServerResponses_,
      [voicePackLanguage]: status,
    };
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'read-anything-app': AppElement;
  }
}

customElements.define(AppElement.is, AppElement);
