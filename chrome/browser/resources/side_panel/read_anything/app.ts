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
import {SkColor} from '//resources/mojo/skia/public/mojom/skcolor.mojom-webui.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';
import {ReadAnythingToolbarElement} from './read_anything_toolbar.js';

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
// removed. Note: if crbug.com/1516972 still exists, these colors will need
// to remain to provide a workaround when color ids are blocked from being
// loaded on first launch.
const style = getComputedStyle(document.body);
const darkThemeBackgroundSkColor =
    rgbToSkColor(style.getPropertyValue('--google-grey-900-rgb'));
const lightThemeBackgroundSkColor =
    rgbToSkColor(style.getPropertyValue('--google-grey-50-rgb'));
const yellowThemeBackgroundSkColor =
    rgbToSkColor(style.getPropertyValue('--google-yellow-100-rgb'));
const blueThemeBackgroundSkColor =
    rgbToSkColor(style.getPropertyValue('--google-blue-100-rgb'));
const lightForegroundSkColor = rgbToSkColor('31,31,31');
const darkForegroundSkColor = rgbToSkColor('227,227,227');
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

export interface ReadAnythingElement {
  $: {
    toolbar: ReadAnythingToolbarElement,
    flexParent: HTMLElement,
  };
}

export class ReadAnythingElement extends ReadAnythingElementBase {
  static get is() {
    return 'read-anything-app';
  }

  static get template() {
    return getTemplate();
  }

  // Defines the valid font names that can be passed to front-end and maps
  // them to a corresponding class style in app.html. Must stay in-sync with
  // the names set in read_anything_font_model.cc.
  private defaultFontName_: string = 'sans-serif';
  private validFontNames_: Array<{name: string, css: string}> = [
    {name: 'Poppins', css: 'Poppins'},
    {name: 'Sans-serif', css: 'sans-serif'},
    {name: 'Serif', css: 'serif'},
    {name: 'Comic Neue', css: '"Comic Neue"'},
    {name: 'Lexend Deca', css: '"Lexend Deca"'},
    {name: 'EB Garamond', css: '"EB Garamond"'},
    {name: 'STIX Two Text', css: '"STIX Two Text"'},
    {name: 'Andika', css: 'Andika'},
  ];

  // Maps a DOM node to the AXNodeID that was used to create it. DOM nodes and
  // AXNodeIDs are unique, so this is a two way map where either DOM node or
  // AXNodeID can be used to access the other.
  private domNodeToAxNodeIdMap_: TwoWayMap<Node, number> = new TwoWayMap();

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

  // State for speech synthesis needs to be tracked separately because there
  // are bugs with window.speechSynthesis.paused and
  // window.speechSynthesis.speaking on some platforms.
  private voice: SpeechSynthesisVoice|undefined;
  paused = true;
  speechStarted = false;
  maxSpeechLength = 175;

  rate: number = 1;

  constructor() {
    super();
    if (chrome.readingMode && chrome.readingMode.isWebUIToolbarVisible) {
      // TODO(crbug.com/1516972): This does not load stylesheets for
      // chrome-untrusted when Chrome is first launched until a new tab is
      // opened. #refreshColorsCss hangs and the Promise never resolves until
      // a new tab is opened. #updateThemeWhenColorTokensAreUnavailable gives
      // a workaround for Reading Mode to allow colors to work when this
      // happens.
      // Longer term, we should investigate if there's a way to force a
      // stylesheet to load when we detect that we've entered the blocked
      // state.
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
      setTimeout(() => chrome.readingMode.shouldShowUI(), 0);
    });

    this.isReadAloudEnabled_ = chrome.readingMode.isReadAloudEnabled;
    if (chrome.readingMode) {
      chrome.readingMode.onConnected();
    }

    this.showLoading();

    document.onselectionchange = () => {
      if (!this.hasContent_) {
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

    if (!chrome.readingMode.linksEnabled && htmlTag === 'a') {
      htmlTag = 'span';
    }

    const element = document.createElement(htmlTag);
    this.domNodeToAxNodeIdMap_.set(element, nodeId);
    const direction = chrome.readingMode.getTextDirection(nodeId);
    if (direction) {
      element.setAttribute('dir', direction);
    }
    const url = chrome.readingMode.getUrl(nodeId);
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

  private appendChildSubtrees_(node: Node, nodeId: number) {
    for (const childNodeId of chrome.readingMode.getChildren(nodeId)) {
      const childNode = this.buildSubtree_(childNodeId);
      node.appendChild(childNode);
    }
  }

  private createTextNode_(nodeId: number): Node {
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
      this.onSpeechFinished();
    }
  }

  // TODO(crbug.com/1474951): Handle focus changes for speech, including
  // updating speech state.
  updateContent() {
    const shadowRoot = this.shadowRoot;
    assert(shadowRoot);
    const container = shadowRoot.getElementById('container');
    assert(container);

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

    this.hasContent_ = true;
    container.appendChild(node);
  }

  updateSelection() {
    const shadowRoot = this.shadowRoot;
    assert(shadowRoot);
    const selection = shadowRoot.getSelection();
    assert(selection);
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

  onSpeechRateChange(rate: number) {
    this.rate = rate;
  }

  getSpeechSynthesisVoice(): SpeechSynthesisVoice|undefined {
    if (!this.voice) {
      this.voice = this.defaultVoice();
    }
    return this.voice;
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
    const voices = this.synth.getVoices().filter(
        voice => voice.lang.startsWith(languageCode));
    if (!voices || (voices.length === 0)) {
      // If no voices in the given language are found, use the default voice.
      return this.synth.getVoices().find(
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

  getVoices(): VoicesByLanguage {
    // TODO(crbug.com/1474951): Filter by localService. Doing this now prevents
    // voices from loading on Linux, which slows down development.
    return this.synth.getVoices().reduce(
        (voicesByLang: VoicesByLanguage, voice: SpeechSynthesisVoice) => {
          (voicesByLang[voice.lang] = voicesByLang[voice.lang] || [])
              .push(voice);
          return voicesByLang;
        },
        {});
  }

  setSpeechSynthesisVoice(voice: SpeechSynthesisVoice|undefined) {
    this.voice = voice;
  }

  previewSpeechSynthesisVoice(voice: SpeechSynthesisVoice) {
    const defaultUtteranceSettings = this.defaultUtteranceSettings();

    // TODO(crbug.com/1474951): Finalize the default voice preview text.
    // TODO(crbug.com/1474951): Call this.synth.cancel() to interrupt reading
    // and reset the play icon.
    const utterance = new SpeechSynthesisUtterance(
        loadTimeData.getString('readingModeVoicePreviewText'));
    utterance.voice = voice;
    utterance.lang = defaultUtteranceSettings.lang;
    utterance.volume = defaultUtteranceSettings.volume;
    utterance.pitch = defaultUtteranceSettings.pitch;
    utterance.rate = defaultUtteranceSettings.rate;

    // TODO(crbug.com/1474951): Add tests for pause button
    utterance.onstart = event => {
      this.$.toolbar.showVoicePreviewPlaying(event.utterance.voice);
    };

    utterance.onend = () => {
      this.$.toolbar.showVoicePreviewDone();
    };

    this.synth.speak(utterance);
  }

  stopSpeech() {
    // TODO(crbug.com/1474951): When pausing, can we pause on the previous
    // word so that speech doesn't resume in the middle of the word?
    this.synth.pause();
    this.paused = true;
  }

  playNextGranularity() {
    this.synth.cancel();
    this.resetPreviousHighlight();
    if (!this.playNextMessage()) {
      this.onSpeechFinished();
    }
  }

  // TODO(crbug.com/1474951): Ensure the highlight is shown after playing the
  //  previous granularity.
  playPreviousGranularity() {
    this.synth.cancel();
    this.resetPreviousHighlight();
    this.playPreviousMessage();
  }

  playSpeech() {
    if (this.speechStarted && this.paused) {
      this.synth.resume();
      this.paused = false;
      return;
    }
    const shadowRoot = this.shadowRoot;
    assert(shadowRoot);
    const container = shadowRoot.getElementById('container');
    assert(container);
    if (container.textContent) {
      this.paused = false;

      // Gather all the messages that can be played. We need nodes, rather
      // than just text because we need to add a span to the current sentence
      // in order to use css styling to highlight the text as it's spoken.
      // Since this modifies the nodes, and we can't do that while we're
      // iterating over the tree, we gather them first, then speak them.
      // TODO(crbug.com/1474951): Better handle if a sentence is split across
      // multiple nodes (e.g. if some text is linked). Right now it will just
      // sound choppy.
      const treeRoot = container.firstChild;
      assert(treeRoot);
      const treeWalker =
          document.createTreeWalker(treeRoot, NodeFilter.SHOW_TEXT);
      treeWalker.nextNode();
      const axNode = this.domNodeToAxNodeIdMap_.get(treeWalker.currentNode);
      // TODO(crbug.com/1474951): There should be a way to use AXPosition so
      // that this step can be skipped.
      if (axNode) {
        chrome.readingMode.initAXPositionWithNode(axNode);
        this.playNextMessage();
      }
    }
  }

  playNextMessage(): boolean {
    const maxTextLength = this.maxSpeechLength;

    // getNextText returns a list of triples of AXNodeIds and start / end text
    // indices, represented as a double array.
    const nextTextIds: number[] = chrome.readingMode.getNextText(maxTextLength);
    return this.playTextOf(nextTextIds);
  }

  playPreviousMessage(): boolean {
    const maxTextLength = this.maxSpeechLength;
    const previousTextIds: number[] =
        chrome.readingMode.getPreviousText(maxTextLength);
    return this.playTextOf(previousTextIds);
  }

  // Play text of these axNodeIds. When finished, call playNextMessage()
  // to read the following text.
  // TODO (crbug.com/1474951): Investigate using AXRange.GetText to get text
  // between start node / end nodes and their offsets.
  private playTextOf(axNodeIds: number[]): boolean {
    const utteranceText = this.extractTextOf(axNodeIds);
    // Return if the utterance is empty or null.
    if (!utteranceText) {
      return false;
    }

    const message = new SpeechSynthesisUtterance(utteranceText);

    message.onerror = (error) => {
      // TODO(crbug.com/1474951): Add more sophisticated error handling.
      if (error.error === 'interrupted') {
        // SpeechSynthesis.cancel() was called, therefore, do nothing.
        return;
      }
      this.synth.cancel();
    };

    message.onend = () => {
      // TODO(crbug.com/1474951): Handle already selected text.
      // TODO(crbug.com/1474951): Return text to its original style once
      // the document has finished.
      this.resetPreviousHighlight();

      // Continue speaking with the next block of text.
      if (!this.playNextMessage()) {
        this.onSpeechFinished();
      }
    };

    // TODO(crbug.com/1474951): Add word callbacks for word highlighting.

    this.highlightNodes(axNodeIds);
    this.speakMessage(message);
    return true;
  }

  private extractTextOf(axNodeIds: number[]): string {
    let utteranceText: string = '';
    for (let i = 0; i < axNodeIds.length; i++) {
      assert(axNodeIds[i]);
      const nodeId = axNodeIds[i];
      const startIndex = chrome.readingMode.getNextTextStartIndex(nodeId);
      const endIndex = chrome.readingMode.getNextTextEndIndex(nodeId);
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

  // TODO(crbug.com/1474951): Handle previous highlighting.
  highlightNodes(nextTextIds: number[]) {
    // implementation based off of #highlightCurrentText below
    assert(nextTextIds.length > 0);
    for (let i = 0; i < nextTextIds.length; i++) {
      const nodeId = nextTextIds[i];
      const element = this.domNodeToAxNodeIdMap_.keyFrom(nodeId);
      if (!element) {
        continue;
      }
      const start = chrome.readingMode.getNextTextStartIndex(nodeId);
      const end = chrome.readingMode.getNextTextEndIndex(nodeId);
      if ((start < 0) || (end < 0)) {
        // If the start or end index is invalid, don't use this node.
        continue;
      }
      let text = element.textContent;
      if (text) {
        text = text.substring(start, end);
      }
      const newElement: Node = this.highlightCurrentText_(start, end, element);
      this.domNodeToAxNodeIdMap_.set(newElement, nodeId);
    }
  }

  speakMessage(message: SpeechSynthesisUtterance) {
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

    this.speechStarted = true;
    this.synth.speak(message);
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
    this.speechStarted = false;
    this.previousHighlight_ = [];
    this.$.toolbar.updateUiForPausing();
  }

  // TODO(b/1465029): Once the IsReadAnythingWebUIEnabled flag is removed
  // this should be renamed to just validatedFontName_ and the current
  // validatedFontName_ method can be removed.
  private validatedFontNameFromName_(fontName: string): string {
    // Validate that the given font name is a valid choice, or use the default.
    const validFontName =
        this.validFontNames_.find((f: {name: string}) => f.name === fontName);
    return validFontName ? validFontName.css : this.defaultFontName_;
  }

  private validatedFontName_(): string {
    return this.validatedFontNameFromName_(chrome.readingMode.fontName);
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

  private resetPreviousHighlight() {
    this.previousHighlight_.forEach((element) => {
      if (element) {
        element.className = previousReadHighlightClass;
      }
    });
  }

  restoreSettingsFromPrefs() {
    if (this.isReadAloudEnabled_) {
      this.onSpeechRateChange(chrome.readingMode.speechRate);
      this.restoreVoiceFromPrefs_();
    }
    this.updateLineSpacing(chrome.readingMode.lineSpacing);
    this.updateLetterSpacing(chrome.readingMode.letterSpacing);
    this.updateFont(chrome.readingMode.fontName);
    this.updateFontSize();
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
      this.setSpeechSynthesisVoice(this.defaultVoice());
      return;
    }

    // TODO(crbug.com/1474951): Ensure various locales are handled such as
    // "en-US" vs. "en-UK." This should be fixed by using page language instead
    // of browser language.
    const voices: VoicesByLanguage = this.getVoices();
    const entry =
        Object.entries(voices).find(([key, _]) => key.startsWith(storedLang));
    let voice;
    if (entry) {
      const voicesForLang: SpeechSynthesisVoice[] = entry[1];
      if (voicesForLang) {
        voice = voicesForLang.find(voice => voice.name === storedVoice);
      }
    }
    this.setSpeechSynthesisVoice(
        (voice === null) ? this.defaultVoice() : voice);
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

  updateFont(fontName: string) {
    const validatedFontName = this.validatedFontNameFromName_(fontName);
    this.updateStyles({
      '--font-family': validatedFontName,
    });

    // Also update the font on the toolbar itself with the validated font name.
    this.$.toolbar.style.fontFamily = validatedFontName;
  }

  updateFontSize() {
    this.updateStyles({
      '--font-size': chrome.readingMode.fontSize + 'em',
    });
  }

  updateHighlight(show: boolean) {
    const highlightBackground =
        this.getCurrentHighlightColorVar(this.currentColorSuffix_);
    this.updateStyles({
      '--current-highlight-bg-color': show ? highlightBackground :
                                             'transparent',
    });
  }

  areColorTokensUnavailable(): boolean {
    // This check is arbitrarily for color-read-anything-text-selection-dark-
    // checking for any color token defined in
    // chrome/browser/ui/color/chrome_color_id.h will work.
    return !window.getComputedStyle(document.documentElement)
                .getPropertyValue('--color-read-anything-text-selection-dark');
  }

  // TODO(crbug.com/1465029): This method should be renamed to updateTheme()
  // and replace the one below once we've removed the Views toolbar.
  updateThemeFromWebUi(colorSuffix: string) {
    this.currentColorSuffix_ = colorSuffix;
    // Check if some property is undefined. If it is, Reading Mode is in a
    // state where stylesheets cannot be loaded without opening a new tab.
    // When this happens, default to using predefined colors. If we do nothing,
    // Reading Mode colors stop working and the overall experience feels broken.
    if (this.areColorTokensUnavailable()) {
      this.updateThemeWhenColorTokensAreUnavailable(colorSuffix);
      return;
    }
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
      return 'var(--color-sys-state-hover-on-subtle)';
    }
    return `var(--color-current-read-aloud-highlight${colorSuffix})`;
  }

  getPreviousHighlightColorVar(colorSuffix: string) {
    if (this.chromeRefresh2023Enabled_ && (colorSuffix === '')) {
      return 'var(--color-sys-on-surface-secondary)';
    }
    return `var(--color-previous-read-aloud-highlight${colorSuffix})`;
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

  // When Chrome is first launched, it's possible for Reading Mode to be opened
  // in a state when stylesheets haven't been loaded and will never be loaded
  // until a new tab is opened and Reading Mode is reopened. When Reading Mode
  // is in this state, color tokens defined in
  // chrome/browser/ui/color/chrome_color_id.h appear as undefined until the
  // new tab is opened. When in this state without a workaround, some colors
  // like selection colors don't work and theme colors cannot be changed. This
  // method serves as a workaround by using colors defined in this file,
  // instead of in chrome_color_id.h.
  // This means that Chrome Refresh colors won't work when the theme is set to
  // default until a new tab is opened, but this is preferable to all colors
  // being broken.
  // This method should only be called in the buggy state. Otherwise,
  // use updateTheme for the Views toolbar or updateThemeForWebUI for the WebUI
  // toolbar.
  // See b/293464821#comment4 or crbug.com/1516972 for more details.
  updateThemeWhenColorTokensAreUnavailable(colorSuffix: string) {
    const foregroundColor =
        this.getForegroundColorForUnavailableColorTokens(colorSuffix);
    const backgroundColor =
        this.getBackgroundColorForUnavailableColorTokens(colorSuffix);
    this.updateThemeWithColors(foregroundColor, backgroundColor);

    // TODO(crbug.com/1474951): Also handle Read Aloud-specific colors, such
    // as the Read Aloud highlights, when in this state.
  }

  getBackgroundColorForUnavailableColorTokens(colorSuffix: string): SkColor {
    if (colorSuffix.includes('light')) {
      return lightThemeBackgroundSkColor;
    }

    if (colorSuffix.includes('dark')) {
      return darkThemeBackgroundSkColor;
    }

    if (colorSuffix.includes('yellow')) {
      return yellowThemeBackgroundSkColor;
    }
    if (colorSuffix.includes('blue')) {
      return blueThemeBackgroundSkColor;
    }

    return lightThemeBackgroundSkColor;
  }

  getForegroundColorForUnavailableColorTokens(colorSuffix: string): SkColor {
    if (colorSuffix.includes('dark')) {
      return darkForegroundSkColor;
    }
    return lightForegroundSkColor;
  }

  updateTheme() {
    const foregroundColor:
        SkColor = {value: chrome.readingMode.foregroundColor};
    const backgroundColor:
        SkColor = {value: chrome.readingMode.backgroundColor};
    this.updateThemeWithColors(foregroundColor, backgroundColor);
  }

  updateThemeWithColors(foregroundColor: SkColor, backgroundColor: SkColor) {
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
      this.$.toolbar.onPlayPauseClick();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'read-anything-app': ReadAnythingElement;
  }
}

customElements.define(ReadAnythingElement.is, ReadAnythingElement);
