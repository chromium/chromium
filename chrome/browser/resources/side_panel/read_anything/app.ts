// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//read-anything-side-panel.top-chrome/shared/sp_empty_state.js';
import '//read-anything-side-panel.top-chrome/shared/sp_shared_style.css.js';
import '//resources/cr_elements/cr_hidden_style.css.js';
import '../strings.m.js';
import './read_anything_toolbar.js';

import {ColorChangeUpdater} from '//resources/cr_components/color_change_listener/colors_css_updater.js';
import {WebUiListenerMixin} from '//resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from '//resources/js/assert.js';
import {rgbToSkColor, skColorToRgba} from '//resources/js/color_utils.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {listenOnce} from '//resources/js/util.js';
import type {SkColor} from '//resources/mojo/skia/public/mojom/skcolor.mojom-webui.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';
import {validatedFontName} from './common.js';
import type {ReadAnythingToolbarElement} from './read_anything_toolbar.js';

const ReadAnythingElementBase = WebUiListenerMixin(PolymerElement);

interface LinkColor {
  default: string;
  visited: string;
}

interface UtteranceSettings {
  lang: string;
  volume: number;
  pitch: number;
  rate: number;
}

interface VoicesByLanguage {
  [lang: string]: SpeechSynthesisVoice[];
}

// TODO(crbug.com/1465029): Remove colors defined here once the Views toolbar is
// removed.
const style = getComputedStyle(document.body);
const darkThemeBackgroundSkColor =
    rgbToSkColor(style.getPropertyValue('--google-grey-900-rgb'));
const lightThemeBackgroundSkColor =
    rgbToSkColor(style.getPropertyValue('--google-grey-50-rgb'));
const yellowThemeBackgroundSkColor =
    rgbToSkColor(style.getPropertyValue('--google-yellow-100-rgb'));
const darkThemeEmptyStateBodyColor = 'var(--google-grey-500)';
const defaultThemeEmptyStateBodyColor = 'var(--google-grey-700)';
const darkThemeLinkColors: LinkColor = {
  default: 'var(--google-blue-300)',
  visited: 'var(--google-purple-200)',
};
const defaultLinkColors: LinkColor = {
  default: 'var(--google-blue-900)',
  visited: 'var(--google-purple-900)',
};
const lightThemeLinkColors: LinkColor = {
  default: 'var(--google-blue-800)',
  visited: 'var(--google-purple-900)',
};
const darkThemeSelectionColor = 'var(--google-blue-200)';
const defaultSelectionColor = 'var(--google-yellow-100)';
const yellowThemeSelectionColor = 'var(--google-blue-100)';

const previousReadHighlightClass = 'previous-read-highlight';

const linkDataAttribute = 'link';

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

/////////////////////////////////////////////////////////////////////
// Called by ReadAnythingUntrustedPageHandler via callback router. //
/////////////////////////////////////////////////////////////////////

// The chrome.readingMode context is created by the ReadAnythingAppController
// which is only instantiated when the kReadAnything feature is enabled. This
// check if chrome.readingMode exists prevents runtime errors when the feature
// is disabled.
if (chrome.readingMode) {
  chrome.readingMode.updateContent = () => {
    const readAnythingApp = document.querySelector('read-anything-app');
    assert(readAnythingApp);
    readAnythingApp.updateContent();
  };

  chrome.readingMode.updateLinks = () => {
    const readAnythingApp = document.querySelector('read-anything-app');
    assert(readAnythingApp);
    readAnythingApp.updateLinks();
  };

  chrome.readingMode.updateImage = (nodeId) => {
    const readAnythingApp = document.querySelector('read-anything-app');
    assert(readAnythingApp);
    readAnythingApp.updateImage(nodeId);
  };

  chrome.readingMode.updateSelection = () => {
    const readAnythingApp = document.querySelector('read-anything-app');
    assert(readAnythingApp);
    readAnythingApp.updateSelection();
  };

  chrome.readingMode.updateTheme = () => {
    const readAnythingApp = document.querySelector('read-anything-app');
    assert(readAnythingApp);
    readAnythingApp.updateTheme();
  };

  chrome.readingMode.showLoading = () => {
    const readAnythingApp = document.querySelector('read-anything-app');
    assert(readAnythingApp);
    readAnythingApp.showLoading();
  };

  chrome.readingMode.showEmpty = () => {
    const readAnythingApp = document.querySelector('read-anything-app');
    assert(readAnythingApp);
    readAnythingApp.showEmpty();
  };

  chrome.readingMode.restoreSettingsFromPrefs = () => {
    const readAnythingApp = document.querySelector('read-anything-app');
    assert(readAnythingApp);
    readAnythingApp.restoreSettingsFromPrefs();
  };

  chrome.readingMode.updateFonts = () => {
    const readAnythingApp = document.querySelector('read-anything-app');
    assert(readAnythingApp);
    readAnythingApp.updateFonts();
  };
}

export enum PauseActionSource {
  DEFAULT,
  BUTTON_CLICK,
  VOICE_PREVIEW,
  VOICE_SETTINGS_CHANGE,
}

export enum WordBoundaryMode {
  NO_BOUNDARIES,
  BOUNDARY_DETECTED,
}

export interface SpeechPlayingState {
  paused: boolean;
  pauseSource?: PauseActionSource;
  speechStarted: boolean;
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

export interface ReadAnythingElement {
  $: {
    toolbar: ReadAnythingToolbarElement,
    flexParent: HTMLElement,
    container: HTMLElement,
  };
}

interface PendingImageRequest {
  resolver: (dataUrl: string) => void;
  cancel: () => void;
  nodeId: number;
}

export class ReadAnythingElement extends ReadAnythingElementBase {
  static get is() {
    return 'read-anything-app';
  }

  static get template() {
    return getTemplate();
  }

  // Maps a DOM node to the AXNodeID that was used to create it. DOM nodes and
  // AXNodeIDs are unique, so this is a two way map where either DOM node or
  // AXNodeID can be used to access the other.
  private domNodeToAxNodeIdMap_: TwoWayMap<Node, number> = new TwoWayMap();
  private imageNodeIdsToFetch_: Set<number> = new Set();
  private pendingImageRequest_?: PendingImageRequest;

  private scrollingOnSelection_: boolean;
  private hasContent_: boolean;
  private emptyStateImagePath_: string;
  private emptyStateDarkImagePath_: string;
  private emptyStateHeading_: string;
  private emptyStateSubheading_: string;

  private previousHighlight_: HTMLElement[] = [];
  private currentColorSuffix_: string;

  private chromeRefresh2023Enabled_ =
      document.documentElement.hasAttribute('chrome-refresh-2023');

  // If the WebUI toolbar should be shown. This happens when the WebUI feature
  // flag is enabled.
  private isWebUIToolbarVisible_: boolean;
  private isReadAloudEnabled_: boolean;

  synth = window.speechSynthesis;

  private selectedVoice: SpeechSynthesisVoice|undefined;

  private availableVoices: SpeechSynthesisVoice[];
  // If a preview is playing, this is set to the voice the preview is playing.
  // Otherwise, this is undefined.
  private previewVoicePlaying: SpeechSynthesisVoice|null;

  private localeToDisplayName: {[locale: string]: string};

  // State for speech synthesis paused/play state needs to be tracked explicitly
  // because there are bugs with window.speechSynthesis.paused and
  // window.speechSynthesis.speaking on some platforms.
  speechPlayingState: SpeechPlayingState = {
    paused: true,
    pauseSource: PauseActionSource.DEFAULT,
    speechStarted: false,
  };

  maxSpeechLength = 175;

  wordBoundaryState: WordBoundaryState = {
    mode: WordBoundaryMode.NO_BOUNDARIES,
    speechUtteranceStartIndex: 0,
    previouslySpokenIndex: 0,
  };

  // The node id of the first text node that should be used by Read Aloud.
  // -1 if the node is not set.
  firstTextNodeSetForReadAloud = -1;

  rate: number = 1;

  constructor() {
    super();
    if (chrome.readingMode && chrome.readingMode.isWebUIToolbarVisible) {
      ColorChangeUpdater.forDocument().start();
    }
  }

  override connectedCallback() {
    super.connectedCallback();

    // Wait until the side panel is fully rendered before showing the side
    // panel. This follows Side Panel best practices and prevents loading
    // artifacts from showing if the side panel is shown before content is
    // ready.
    listenOnce(this.$.flexParent, 'dom-change', () => {
      setTimeout(() => chrome.readingMode.shouldShowUi(), 0);
    });

    this.isReadAloudEnabled_ = chrome.readingMode.isReadAloudEnabled;
    if (chrome.readingMode) {
      chrome.readingMode.onConnected();
    }

    this.showLoading();

    document.onselectionchange = () => {
      // When Read Aloud is playing, user-selection is disabled on the Read
      // Anything panel, so don't attempt to update selection, as this can
      // end up clearing selection in the main part of the browser.
      if (!this.hasContent_ || !this.speechPlayingState.paused) {
        return;
      }
      const shadowRoot = this.shadowRoot;
      assert(shadowRoot);
      const selection = shadowRoot.getSelection();
      assert(selection);
      const {anchorNode, anchorOffset, focusNode, focusOffset} = selection;
      if (!anchorNode || !focusNode) {
        // The selection was collapsed by clicking inside the selection.
        chrome.readingMode.onCollapseSelection();
        return;
      }
      const anchorNodeId = this.domNodeToAxNodeIdMap_.get(anchorNode);
      const focusNodeId = this.domNodeToAxNodeIdMap_.get(focusNode);
      assert(anchorNodeId && focusNodeId);
      chrome.readingMode.onSelectionChange(
          anchorNodeId, anchorOffset, focusNodeId, focusOffset);
    };

    document.onscroll = () => {
      chrome.readingMode.onScroll(this.scrollingOnSelection_);
      this.scrollingOnSelection_ = false;
    };

    // Pass copy commands to main page. Copy commands will not work if they are
    // disabled on the main page.
    document.oncopy = () => {
      chrome.readingMode.onCopy();
      return false;
    };

    this.isWebUIToolbarVisible_ = chrome.readingMode.isWebUIToolbarVisible;
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
    if (chrome.readingMode.isGoogleDocs() &&
        chrome.readingMode.isLeafNode(nodeId)) {
      return this.createTextNode_(nodeId);
    }

    // getHtmlTag might return '#document' which is not a valid to pass to
    // createElement.
    if (htmlTag === '#document') {
      htmlTag = 'div';
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

    if (element.nodeName === 'IMG') {
      const dataUrl = chrome.readingMode.getImageDataUrl(nodeId);
      if (!dataUrl) {
        this.imageNodeIdsToFetch_.add(nodeId);
      }
      element.setAttribute('src', dataUrl);
      const altText = chrome.readingMode.getAltText(nodeId);
      element.setAttribute('alt', altText);
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

  // TODO(crbug.com/1442693): Potentially hide links during distillation.
  private shouldShowLinks(): boolean {
    // Links should only show when Read Aloud is paused.
    return chrome.readingMode.linksEnabled && this.speechPlayingState.paused;
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
    // However, since updateContent may be called after speech starts playing,
    // don't call InitAXPosition from here to avoid interrupting current speech.
    if (this.firstTextNodeSetForReadAloud < 0) {
      this.firstTextNodeSetForReadAloud = nodeId;
    }

    const textContent = chrome.readingMode.getTextContent(nodeId);
    const textNode = document.createTextNode(textContent);
    this.domNodeToAxNodeIdMap_.set(textNode, nodeId);
    const isOverline = chrome.readingMode.isOverline(nodeId);
    let shouldBold = chrome.readingMode.shouldBold(nodeId);

    if (chrome.readingMode.isGoogleDocs()) {
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
    if (chrome.readingMode.isSelectable) {
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

  // TODO(crbug.com/1474951): Handle focus changes for speech, including
  // updating speech state.
  updateContent() {
    // Each time we rebuild the subtree, we should clear the node id of the
    // first text node.
    this.firstTextNodeSetForReadAloud = -1;
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

    const node = this.buildSubtree_(rootId);
    if (!node.textContent) {
      return;
    }

    this.loadImages_();

    this.hasContent_ = true;
    container.appendChild(node);
  }

  updateImage(nodeId: number) {
    const dataurl = chrome.readingMode.getImageDataUrl(nodeId);
    if (this.pendingImageRequest_ &&
        this.pendingImageRequest_.nodeId === nodeId) {
      this.pendingImageRequest_.resolver(dataurl);
    }
  }

  private async loadImages_() {
    // Content was updated while a request was still pending.
    if (this.pendingImageRequest_) {
      this.pendingImageRequest_.cancel();
    }

    for (const nodeId of this.imageNodeIdsToFetch_) {
      // Create a promise that will be resolved on image updated.
      try {
        const dataUrl = await new Promise<string>((resolve, reject) => {
          this.pendingImageRequest_ = {
            resolver: resolve,
            cancel: reject,
            nodeId: nodeId,
          };
          chrome.readingMode.requestImageDataUrl(nodeId);
        });
        const node = this.domNodeToAxNodeIdMap_.keyFrom(nodeId);
        if (node instanceof HTMLImageElement) {
          node.src = dataUrl;
        }
      } catch {
        // This catch will be called if cancel is called on the image request.
        this.pendingImageRequest_ = undefined;
        break;
      }
    }
    this.imageNodeIdsToFetch_.clear();
  }

  getSelection(): any {
    const shadowRoot = this.shadowRoot;
    assert(shadowRoot);
    const selection = shadowRoot.getSelection();
    return selection;
  }

  updateSelection() {
    const selection = this.getSelection()!;
    selection.removeAllRanges();

    const range = new Range();
    const startNodeId = chrome.readingMode.startNodeId;
    const startOffset = chrome.readingMode.startOffset;
    const endNodeId = chrome.readingMode.endNodeId;
    const endOffset = chrome.readingMode.endOffset;
    const startNode = this.domNodeToAxNodeIdMap_.keyFrom(startNodeId);
    const endNode = this.domNodeToAxNodeIdMap_.keyFrom(endNodeId);
    if (!startNode || !endNode) {
      return;
    }
    range.setStart(startNode, startOffset);
    range.setEnd(endNode, endOffset);
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

  updateLinks() {
    if (!this.shadowRoot) {
      return;
    }

    const selector = this.shouldShowLinks() ? 'span[data-link]' : 'a';
    const elements = this.shadowRoot.querySelectorAll(selector);

    for (const elem of elements) {
      assert(elem instanceof HTMLElement);
      const nodeId = this.domNodeToAxNodeIdMap_.get(elem);
      assert(nodeId !== undefined);
      const replacement = this.buildSubtree_(nodeId);
      this.replaceElement(elem, replacement);
    }
  }

  private onSpeechRateChange_(event: CustomEvent<{rate: number}>) {
    this.updateSpeechRate_(event.detail.rate);
  }

  private updateSpeechRate_(rate: number) {
    this.rate = rate;
    this.resetSpeechPostSettingChange_();
  }

  getSpeechSynthesisVoice(): SpeechSynthesisVoice|undefined {
    if (!this.selectedVoice) {
      this.selectedVoice = this.defaultVoice();
    }
    return this.selectedVoice;
  }

  defaultVoice(): SpeechSynthesisVoice|undefined {
    // TODO(crbug.com/1474951): Additional logic to find default voice if there
    // isn't a voice marked as default

    // TODO(crbug.com/1474951): Filter by localService. Doing this now prevents
    // voices from loading on Linux, which slows down development.
    const languageCode = chrome.readingMode.speechSynthesisLanguageCode;
    // TODO(crbug.com/1474951): Ensure various locales are handled such as
    // "en-US" vs. "en-UK." This should be fixed by using page language instead
    // of browser language.
    const voices =
        this.getVoices().filter(voice => voice.lang.startsWith(languageCode));

    if (!voices || (voices.length === 0)) {
      // If no voices in the given language are found, use the default voice.
      return this.getVoices().find(
          ({default: isDefaultVoice}) => isDefaultVoice);
    }

    // The default voice doesn't always match with the actual default voice
    // of the device, therefore use the language code to find a voice first.
    const voice = voices.find(({default: isDefaultVoice}) => isDefaultVoice);
    if (!voice) {
      return voices[0];
    }

    return voice;
  }

  private getVoicesByLanguage(): VoicesByLanguage {
    // TODO(crbug.com/1474951): Filter by localService. Doing this now prevents
    // voices from loading on Linux, which slows down development.
    return this.getVoices().reduce(
        (voicesByLang: VoicesByLanguage, voice: SpeechSynthesisVoice) => {
          (voicesByLang[voice.lang] = voicesByLang[voice.lang] || [])
              .push(voice);
          return voicesByLang;
        },
        {});
  }

  private getVoices(): SpeechSynthesisVoice[] {
    if (!this.availableVoices) {
      this.availableVoices = this.synth.getVoices();
      this.populateDisplayNamesForLocaleCodes();
    }
    return this.availableVoices;
  }

  private populateDisplayNamesForLocaleCodes() {
    this.localeToDisplayName = {};

    for (const {lang} of this.availableVoices) {
      if (!(lang in this.localeToDisplayName)) {
        const langDisplayName =
            chrome.readingMode.getDisplayNameForLocale(lang, lang);
        if (langDisplayName) {
          this.localeToDisplayName =
              {...this.localeToDisplayName, [lang]: langDisplayName};
        }
      }
    }
  }

  private replaceElement(current: HTMLElement, replacer: Node) {
    const nodeId = this.domNodeToAxNodeIdMap_.get(current);
    assert(nodeId !== undefined);
    // Update map.
    this.domNodeToAxNodeIdMap_.delete(current);
    this.domNodeToAxNodeIdMap_.set(replacer, nodeId);
    // Replace element in DOM.
    current.replaceWith(replacer);
  }

  private onPreviewVoice_(
      event: CustomEvent<{previewVoice: SpeechSynthesisVoice}>) {
    event.preventDefault();
    event.stopPropagation();

    this.stopSpeech(PauseActionSource.VOICE_PREVIEW);

    const defaultUtteranceSettings = this.defaultUtteranceSettings();

    // TODO(crbug.com/1474951): Finalize the default voice preview text.
    const utterance = new SpeechSynthesisUtterance(
        loadTimeData.getString('readingModeVoicePreviewText'));
    const voice = event.detail.previewVoice;
    utterance.voice = voice;
    utterance.lang = defaultUtteranceSettings.lang;
    utterance.volume = defaultUtteranceSettings.volume;
    utterance.pitch = defaultUtteranceSettings.pitch;
    utterance.rate = defaultUtteranceSettings.rate;

    // TODO(crbug.com/1474951): Add tests for pause button
    utterance.onstart = event => {
      this.previewVoicePlaying = event.utterance.voice;
    };

    utterance.onend = () => {
      this.previewVoicePlaying = null;
    };

    this.synth.speak(utterance);
  }

  private onVoiceMenuClose_(
      event: CustomEvent<{voicePlayingWhenMenuOpened: boolean}>) {
    event.preventDefault();
    event.stopPropagation();

    // TODO(b/323912186) Handle when menu is closed mid-preview and the user
    // presses play/pause button.
    if (this.speechPlayingState.paused &&
        event.detail.voicePlayingWhenMenuOpened) {
      this.playSpeech();
    }
  }
  private onPlayPauseClick_() {
    if (this.speechPlayingState.paused) {
      this.playSpeech();
    } else {
      this.stopSpeech(PauseActionSource.BUTTON_CLICK);
    }
  }

  stopSpeech(pauseSource: PauseActionSource) {
    // TODO(crbug.com/1474951): When pausing, can we pause on a word boundary
    // and continue playing from the previous word?
    this.speechPlayingState = {
      ...this.speechPlayingState,
      paused: true,
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
      this.updateLinks();
      this.highlightNodes(chrome.readingMode.getCurrentText());
    }
  }

  private playNextGranularity_() {
    this.synth.cancel();
    this.resetPreviousHighlight();
    // Reset the word boundary index whenever we move the granularity position.
    this.resetToDefaultWordBoundaryState();
    chrome.readingMode.movePositionToNextGranularity();

    if (!this.highlightAndPlayMessage()) {
      this.onSpeechFinished();
    }
  }

  // TODO(crbug.com/1474951): Ensure the highlight is shown after playing the
  //  previous granularity.
  private playPreviousGranularity_() {
    this.synth.cancel();
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
    if (this.speechPlayingState.speechStarted &&
        this.speechPlayingState.paused) {
      const pausedFromButton = this.speechPlayingState.pauseSource ===
          PauseActionSource.BUTTON_CLICK;

      // If word boundaries aren't supported for the given voice, we should
      // still continue to use synth.resume, as this is preferable to
      // restarting the current message.
      if (pausedFromButton &&
          this.wordBoundaryState.mode !== WordBoundaryMode.BOUNDARY_DETECTED) {
        this.synth.resume();
      } else {
        this.synth.cancel();
        this.highlightAndPlayInterruptedMessage();
      }

      this.speechPlayingState = {paused: false, speechStarted: true};

      // Hide links when speech resumes. We only hide links when the page was
      // paused from the play/pause button.
      if (chrome.readingMode.linksEnabled && pausedFromButton) {
        this.updateLinks();
      }

      // If the current read highlight has been cleared from a call to
      // updateContent, such as for links being toggled on or off via a Read
      // Aloud play / pause or via a preference change, rehighlight the nodes
      // after a pause.
      if (!container.querySelector('.current-read-highlight')) {
        // TODO(crbug.com/1474951): Investigate adding a mock voice in tests
        // to make this testable.
        this.highlightNodes(chrome.readingMode.getCurrentText());
      }

      return;
    }
    if (container.textContent) {
      this.speechPlayingState = {paused: false, speechStarted: true};
      // Hide links when speech begins playing.
      if (chrome.readingMode.linksEnabled) {
        this.updateLinks();
      }

      // TODO(crbug.com/1474951): There should be a way to use AXPosition so
      // that this step can be skipped.
      if (this.firstTextNodeSetForReadAloud > 0) {
        chrome.readingMode.initAxPositionWithNode(
            this.firstTextNodeSetForReadAloud);
        this.highlightAndPlayMessage();
      }
    }
  }

  // TODO: Should this be merged with highlightAndPlayMessage?
  highlightAndPlayInterruptedMessage() {
    // getCurrentText gets the AX Node IDs of text that should be spoken and
    // highlighted.
    const axNodeIds: number[] = chrome.readingMode.getCurrentText();

    const utteranceText = this.extractTextOf(axNodeIds);
    // Return if the utterance is empty or null.
    if (!utteranceText) {
      return false;
    }

    if (this.wordBoundaryState.mode === WordBoundaryMode.BOUNDARY_DETECTED) {
      const substringIndex = this.wordBoundaryState.previouslySpokenIndex +
          this.wordBoundaryState.speechUtteranceStartIndex;
      this.wordBoundaryState.previouslySpokenIndex = 0;
      this.wordBoundaryState.speechUtteranceStartIndex = substringIndex;
      this.playText(utteranceText.substring(substringIndex));
    } else {
      this.playText(utteranceText);
    }
    this.highlightNodes(axNodeIds);
    return true;
  }

  // Play text of these axNodeIds. When finished, read and highlight to read the
  // following text.
  // TODO (crbug.com/1474951): Investigate using AXRange.GetText to get text
  // between start node / end nodes and their offsets.
  highlightAndPlayMessage(): boolean {
    // getCurrentText gets the AX Node IDs of text that should be spoken and
    // highlighted.
    const axNodeIds: number[] = chrome.readingMode.getCurrentText();

    const utteranceText = this.extractTextOf(axNodeIds);
    // Return if the utterance is empty or null.
    if (!utteranceText) {
      return false;
    }

    this.playText(utteranceText);
    this.highlightNodes(axNodeIds);
    return true;
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

    // TODO(crbug.com/1474951): getAccessibleBoundary breaks on the nearest
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
    const isTextTooLong = this.selectedVoice?.localService ?
        false :
        utteranceText.length > this.maxSpeechLength;
    const endBoundary = isTextTooLong ?
        this.getAccessibleTextLength(utteranceText) :
        utteranceText.length;
    const message =
        new SpeechSynthesisUtterance(utteranceText.substring(0, endBoundary));

    message.onerror = (error) => {
      // TODO(crbug.com/1474951): Add more sophisticated error handling.
      if (error.error === 'interrupted') {
        // SpeechSynthesis.cancel() was called, therefore, do nothing.
        return;
      }
      this.synth.cancel();
    };

    message.addEventListener('boundary', (event) => {
      // Some voices may give sentence boundaries, but we're only concerned
      // with word boundaries in boundary event because we're speaking text at
      // the sentence granularity level, so we'll retrieve these boundaries in message.onEnd
      // instead.
      if (event.name === 'word') {
        this.updateBoundary(event.charIndex);
      }
    });

    message.onend = () => {
      if (isTextTooLong) {
        // Since our previous utterance was too long, continue speaking pieces
        // of the current utterance until the utterance is complete. The entire
        // utterance is highlighted, so there's no need to update highlighting
        // until the utterance substring is an acceptable size.
        this.playText(utteranceText.substring(endBoundary));
        return;
      }
      // TODO(crbug.com/1474951): Handle already selected text.
      // TODO(crbug.com/1474951): Return text to its original style once
      // the document has finished.
      this.resetPreviousHighlight();

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

    // TODO(crbug.com/1474951): Add word callbacks for word highlighting.

    const voice = this.getSpeechSynthesisVoice();
    if (!voice) {
      // TODO(crbug.com/1474951): Handle when no voices are available.
      return;
    }

    message.voice = voice;

    const utteranceSettings = this.defaultUtteranceSettings();
    message.lang = utteranceSettings.lang;
    message.volume = utteranceSettings.volume;
    message.pitch = utteranceSettings.pitch;
    message.rate = utteranceSettings.rate;

    this.synth.speak(message);
  }

  updateBoundary(charIndex: number) {
    this.wordBoundaryState.previouslySpokenIndex = charIndex;
    this.wordBoundaryState.mode = WordBoundaryMode.BOUNDARY_DETECTED;
  }

  resetToDefaultWordBoundaryState() {
    this.wordBoundaryState = {
      previouslySpokenIndex: 0,
      mode: WordBoundaryMode.NO_BOUNDARIES,
      speechUtteranceStartIndex: 0,
    };
  }

  private extractTextOf(axNodeIds: number[]): string {
    let utteranceText: string = '';
    for (let i = 0; i < axNodeIds.length; i++) {
      assert(axNodeIds[i]);
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
        utteranceText += ' ' + content;
      }
    }
    return utteranceText;
  }

  highlightNodes(nextTextIds: number[]) {
    if (nextTextIds.length === 0) {
      return;
    }

    for (let i = 0; i < nextTextIds.length; i++) {
      const nodeId = nextTextIds[i];
      const element = this.domNodeToAxNodeIdMap_.keyFrom(nodeId);
      if (!element) {
        continue;
      }
      const start = chrome.readingMode.getCurrentTextStartIndex(nodeId);
      const end = chrome.readingMode.getCurrentTextEndIndex(nodeId);
      if ((start < 0) || (end < 0)) {
        // If the start or end index is invalid, don't use this node.
        continue;
      }
      const newElement: Node = this.highlightCurrentText_(start, end, element);
      this.domNodeToAxNodeIdMap_.set(newElement, nodeId);
    }
  }

  private defaultUtteranceSettings(): UtteranceSettings {
    // TODO(crbug.com/1474951): Use correct locale when speaking.
    const lang = chrome.readingMode.speechSynthesisLanguageCode;

    return {
      lang,
      // TODO(crbug.com/1474951): Ensure rate change happens immediately, rather
      // than on the next set of text.
      // TODO(crbug.com/1474951): Ensure the rate is valid for the current
      // speech engine.
      rate: this.rate,
      // TODO(crbug.com/1474951): Ensure the correct default values are used.
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
  // and returns the top-level span node
  private highlightCurrentText_(
      toHighlightStart: number, toHighlightEnd: number,
      currentNode: Node): Node {
    const parentOfHighlight = document.createElement('span');

    // First pull out any text within this node before the highlighted section.
    // Since it's already been highlighted, we fade it out.
    const highlightPrefix =
        currentNode.textContent!.substring(0, toHighlightStart);
    if (highlightPrefix.length > 0) {
      const prefixNode = document.createElement('span');
      prefixNode.className = previousReadHighlightClass;
      prefixNode.textContent = highlightPrefix;
      parentOfHighlight.appendChild(prefixNode);
    }

    // Then get the section of text to highlight and mark it for
    // highlighting.
    const readingHighlight = document.createElement('span');
    readingHighlight.className = 'current-read-highlight';
    readingHighlight.textContent =
        currentNode.textContent!.substring(toHighlightStart, toHighlightEnd);
    parentOfHighlight.appendChild(readingHighlight);

    // Finally, append the rest of the text for this node that has yet to be
    // highlighted.
    const highlightSuffix = currentNode.textContent!.substring(toHighlightEnd);
    if (highlightSuffix.length > 0) {
      const suffixNode = document.createTextNode(highlightSuffix);
      parentOfHighlight.appendChild(suffixNode);
    }

    // Replace the current node in the tree with the split up version of the
    // node.
    this.previousHighlight_.push(readingHighlight);
    if (currentNode.parentNode) {
      currentNode.parentNode.replaceChild(parentOfHighlight, currentNode);
    }

    // Automatically scroll the text so the highlight stays roughly centered.
    readingHighlight.scrollIntoViewIfNeeded();
    return parentOfHighlight;
  }

  private onSpeechFinished() {
    this.clearReadAloudState();

    // Hide links when speech finishes playing.
    if (chrome.readingMode.linksEnabled) {
      this.updateLinks();
    }
  }

  private clearReadAloudState() {
    this.speechPlayingState = {
      paused: true,
      pauseSource: PauseActionSource.DEFAULT,
      speechStarted: false,
    };
    this.previousHighlight_ = [];
    this.resetToDefaultWordBoundaryState();
  }

  private onSelectVoice_(
      event: CustomEvent<{selectedVoice: SpeechSynthesisVoice}>) {
    event.preventDefault();
    event.stopPropagation();

    this.selectedVoice = event.detail.selectedVoice;
    chrome.readingMode.onVoiceChange(
        this.selectedVoice.name, this.selectedVoice.lang.split('-')[0]);

    this.resetSpeechPostSettingChange_();
  }

  private resetSpeechPostSettingChange_() {
    // Don't call stopSpeech() if initAxPositionWithNode hasn't been called
    if (!this.speechPlayingState.speechStarted) {
      return;
    }

    const playSpeechOnChange = !this.speechPlayingState.paused;

    // Cancel the queued up Utterance using the old speech settings
    this.stopSpeech(PauseActionSource.VOICE_SETTINGS_CHANGE);

    // If speech was playing when a setting was changed, continue playing speech
    if (playSpeechOnChange) {
      this.playSpeech();
    }
  }

  // TODO(b/1465029): Once the IsReadAnythingWebUIEnabled flag is removed
  // this should be removed
  private validatedFontName_(): string {
    return validatedFontName(chrome.readingMode.fontName);
  }

  private getLinkColor_(backgroundSkColor: SkColor): LinkColor {
    switch (backgroundSkColor.value) {
      case darkThemeBackgroundSkColor.value:
        return darkThemeLinkColors;
      case lightThemeBackgroundSkColor.value:
        return lightThemeLinkColors;
      default:
        return defaultLinkColors;
    }
  }

  private getEmptyStateBodyColor_(backgroundSkColor: SkColor): string {
    const isDark = backgroundSkColor.value === darkThemeBackgroundSkColor.value;
    return isDark ? darkThemeEmptyStateBodyColor :
                    defaultThemeEmptyStateBodyColor;
  }

  // TODO(crbug.com/1465029): This method should be renamed to
  // getEmptyStateBodyColor_() and replace the one above once we've removed the
  // Views toolbar.
  private getEmptyStateBodyColorFromWebUi_(colorSuffix: string): string {
    const isDark = colorSuffix.includes('dark');
    return isDark ? darkThemeEmptyStateBodyColor :
                    defaultThemeEmptyStateBodyColor;
  }

  private getSelectionColor_(backgroundSkColor: SkColor): string {
    switch (backgroundSkColor.value) {
      case darkThemeBackgroundSkColor.value:
        return darkThemeSelectionColor;
      case yellowThemeBackgroundSkColor.value:
        return yellowThemeSelectionColor;
      default:
        return defaultSelectionColor;
    }
  }

  private resetPreviousHighlightAndRemoveCurrentHighlight() {
    const lastElement = this.previousHighlight_.pop();
    if (lastElement) {
      lastElement.className = '';
    }

    this.resetPreviousHighlight();
  }

  private resetPreviousHighlight() {
    this.previousHighlight_.forEach((element) => {
      if (element) {
        element.className = previousReadHighlightClass;
      }
    });
  }

  restoreSettingsFromPrefs() {
    if (this.isReadAloudEnabled_) {
      this.updateSpeechRate_(chrome.readingMode.speechRate);
      this.restoreVoiceFromPrefs_();
    }
    this.updateLineSpacing(chrome.readingMode.lineSpacing);
    this.updateLetterSpacing(chrome.readingMode.letterSpacing);
    this.updateFont_(chrome.readingMode.fontName);
    this.updateFontSize_();
    let colorSuffix: string|undefined;
    switch (chrome.readingMode.colorTheme) {
      case chrome.readingMode.defaultTheme:
        colorSuffix = '';
        break;
      case chrome.readingMode.lightTheme:
        colorSuffix = '-light';
        break;
      case chrome.readingMode.darkTheme:
        colorSuffix = '-dark';
        break;
      case chrome.readingMode.yellowTheme:
        colorSuffix = '-yellow';
        break;
      case chrome.readingMode.blueTheme:
        colorSuffix = '-blue';
        break;
      default:
        // Do nothing
    }
    if (colorSuffix !== undefined) {
      this.updateThemeFromWebUi(colorSuffix);
    }
    // TODO(crbug.com/1474951): investigate using parent/child relationshiop
    // instead of element by id.
    this.$.toolbar.restoreSettingsFromPrefs(colorSuffix);
  }

  private restoreVoiceFromPrefs_() {
    const storedLang = chrome.readingMode.speechSynthesisLanguageCode;
    const storedVoice = chrome.readingMode.getStoredVoice(storedLang);

    if (!storedVoice) {
      this.selectedVoice = this.defaultVoice();
      return;
    }

    // TODO(crbug.com/1474951): Ensure various locales are handled such as
    // "en-US" vs. "en-UK." This should be fixed by using page language instead
    // of browser language.
    const voices: VoicesByLanguage = this.getVoicesByLanguage();
    const entry =
        Object.entries(voices).find(([key, _]) => key.startsWith(storedLang));
    let voice;
    if (entry) {
      const voicesForLang: SpeechSynthesisVoice[] = entry[1];
      if (voicesForLang) {
        voice = voicesForLang.find(voice => voice.name === storedVoice);
      }
    }
    this.selectedVoice = (voice === null) ? this.defaultVoice() : voice;
  }

  updateLineSpacing(newLineHeight: number) {
    this.updateStyles({
      '--line-height': newLineHeight,
    });
  }

  updateLetterSpacing(newLetterSpacing: number) {
    this.updateStyles({
      '--letter-spacing': newLetterSpacing + 'em',
    });
  }

  private onFontChange_(event: CustomEvent<{fontName: string}>) {
    this.updateFont_(event.detail.fontName);
  }

  private updateFont_(fontName: string) {
    const validFontName = validatedFontName(fontName);
    this.updateStyles({
      '--font-family': validFontName,
    });
  }

  private updateFontSize_() {
    this.updateStyles({
      '--font-size': chrome.readingMode.fontSize + 'em',
    });
  }

  private onHighlightToggle_(event: CustomEvent<{highlightOn: boolean}>) {
    const highlightBackground =
        this.getCurrentHighlightColorVar(this.currentColorSuffix_);
    this.updateStyles({
      '--current-highlight-bg-color':
          event.detail.highlightOn ? highlightBackground : 'transparent',
    });
  }

  // TODO(crbug.com/1465029): This method should be renamed to updateTheme()
  // and replace the one below once we've removed the Views toolbar.
  updateThemeFromWebUi(colorSuffix: string) {
    this.currentColorSuffix_ = colorSuffix;
    const emptyStateBodyColor = colorSuffix ?
        this.getEmptyStateBodyColorFromWebUi_(colorSuffix) :
        'var(--color-side-panel-card-secondary-foreground)';
    this.updateStyles({
      '--background-color': this.getBackgroundColorVar(colorSuffix),
      '--foreground-color': this.getForegroundColorVar(colorSuffix),
      '--selection-color': this.getSelectionColorVar(colorSuffix),
      '--current-highlight-bg-color':
          this.getCurrentHighlightColorVar(colorSuffix),
      '--previous-highlight-color':
          this.getPreviousHighlightColorVar(colorSuffix),
      '--sp-empty-state-heading-color':
          `var(--color-read-anything-foreground${colorSuffix})`,
      '--sp-empty-state-body-color': emptyStateBodyColor,
      '--link-color': `var(--color-read-anything-link-default${colorSuffix})`,
      '--visited-link-color':
          `var(--color-read-anything-link-visited${colorSuffix})`,
    });
    document.documentElement.style.setProperty(
        '--selection-color', this.getSelectionColorVar(colorSuffix));
    document.documentElement.style.setProperty(
        '--selection-text-color', this.getSelectionTextColorVar(colorSuffix));
  }

  getCurrentHighlightColorVar(colorSuffix: string) {
    if (this.chromeRefresh2023Enabled_ && (colorSuffix === '')) {
      return 'var(--color-sys-state-hover-dim-blend-protection)';
    }
    return `var(--color-read-anything-current-read-aloud-highlight${
        colorSuffix})`;
  }

  getPreviousHighlightColorVar(colorSuffix: string) {
    if (this.chromeRefresh2023Enabled_ && (colorSuffix === '')) {
      return 'var(--color-sys-on-surface-secondary)';
    }
    return `var(--color-read-anything-previous-read-aloud-highlight${
        colorSuffix})`;
  }

  getBackgroundColorVar(colorSuffix: string) {
    if (this.chromeRefresh2023Enabled_ && (colorSuffix === '')) {
      return 'var(--color-sys-base-container-elevated)';
    }
    return `var(--color-read-anything-background${colorSuffix})`;
  }

  getForegroundColorVar(colorSuffix: string) {
    if (this.chromeRefresh2023Enabled_ && (colorSuffix === '')) {
      return 'var(--color-sys-on-surface)';
    }
    return `var(--color-read-anything-foreground${colorSuffix})`;
  }

  getSelectionColorVar(colorSuffix: string) {
    if (this.chromeRefresh2023Enabled_ && (colorSuffix === '')) {
      return 'var(--color-text-selection-background)';
    }
    return `var(--color-read-anything-text-selection${colorSuffix})`;
  }

  getSelectionTextColorVar(colorSuffix: string) {
    if (this.chromeRefresh2023Enabled_ && (colorSuffix === '')) {
      return 'var(--color-text-selection-foreground)';
    }

    if (window.matchMedia('(prefers-color-schme: dark)').matches) {
      return `var(--google-grey-900)`;
    }

    return `var(--google-grey-800)`;
  }

  updateTheme() {
    const foregroundColor:
        SkColor = {value: chrome.readingMode.foregroundColor};
    const backgroundColor:
        SkColor = {value: chrome.readingMode.backgroundColor};
    const linkColor = this.getLinkColor_(backgroundColor);

    this.updateStyles({
      '--background-color': skColorToRgba(backgroundColor),
      '--font-family': this.validatedFontName_(),
      '--font-size': chrome.readingMode.fontSize + 'em',
      '--foreground-color': skColorToRgba(foregroundColor),
      '--letter-spacing': chrome.readingMode.letterSpacing + 'em',
      '--line-height': chrome.readingMode.lineSpacing,
      '--link-color': linkColor.default,
      '--selection-color': this.getSelectionColor_(backgroundColor),
      '--sp-empty-state-heading-color': skColorToRgba(foregroundColor),
      '--sp-empty-state-body-color':
          this.getEmptyStateBodyColor_(backgroundColor),
      '--visited-link-color': linkColor.visited,
    });
    if (!chrome.readingMode.isWebUIToolbarVisible) {
      document.body.style.background = skColorToRgba(backgroundColor);
    }

    document.documentElement.style.setProperty(
        '--selection-color', this.getSelectionColor_(backgroundColor));
    document.documentElement.style.setProperty(
        '--selection-text-color',
        this.getSelectionTextColorVar(skColorToRgba(backgroundColor)));
  }

  updateFonts() {
    // Also update the font on the toolbar itself with the validated font name.
    this.$.toolbar.updateFonts();
  }

  private onKeyDown_(e: KeyboardEvent) {
    if (e.key === 'k') {
      e.stopPropagation();
      this.onPlayPauseClick_();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'read-anything-app': ReadAnythingElement;
  }
}

customElements.define(ReadAnythingElement.is, ReadAnythingElement);
