// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//read-anything-side-panel.top-chrome/shared/sp_empty_state.js';
import '//read-anything-side-panel.top-chrome/shared/sp_shared_style.css.js';
import '//resources/cr_elements/cr_hidden_style.css.js';
import '//resources/cr_elements/cr_shared_vars.css.js';
import '//resources/cr_elements/cr_toast/cr_toast.js';
import './strings.m.js';
import './read_anything_toolbar.js';

import {ColorChangeUpdater} from '//resources/cr_components/color_change_listener/colors_css_updater.js';
import type {CrToastElement} from '//resources/cr_elements/cr_toast/cr_toast.js';
import {WebUiListenerMixin} from '//resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from '//resources/js/assert.js';
import {rgbToSkColor, skColorToRgba} from '//resources/js/color_utils.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {listenOnce} from '//resources/js/util.js';
import type {SkColor} from '//resources/mojo/skia/public/mojom/skcolor.mojom-webui.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';
import {minOverflowLengthToScroll, validatedFontName} from './common.js';
import type {ReadAnythingToolbarElement} from './read_anything_toolbar.js';
import {areVoicesEqual, AVAILABLE_GOOGLE_TTS_LOCALES, convertLangOrLocaleForVoicePackManager, convertLangToAnAvailableLangIfPresent, createInitialListOfEnabledLanguages, mojoVoicePackStatusToVoicePackStatusEnum, VoicePackStatus} from './voice_language_util.js';

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

// TODO(crbug.com/40275871): Remove colors defined here once the Views toolbar
// is removed.
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

export const previousReadHighlightClass = 'previous-read-highlight';
export const currentReadHighlightClass = 'current-read-highlight';
const parentOfHighlightClass = 'parent-of-highlight';

const linkDataAttribute = 'link';

// Constants for styling the app when page zoom changes.
const overflowXTypical = 'hidden';
const overflowXScroll = 'scroll';
const minWidthTypical = 'auto';
const minWidthOverflow = 'fit-content';

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
    assert(readAnythingApp, 'no app');
    readAnythingApp.updateContent();
  };

  chrome.readingMode.updateLinks = () => {
    const readAnythingApp = document.querySelector('read-anything-app');
    assert(readAnythingApp, 'no app');
    readAnythingApp.updateLinks();
  };

  chrome.readingMode.updateImage = (nodeId) => {
    const readAnythingApp = document.querySelector('read-anything-app');
    assert(readAnythingApp, 'no app');
    readAnythingApp.updateImage(nodeId);
  };

  chrome.readingMode.updateSelection = () => {
    const readAnythingApp = document.querySelector('read-anything-app');
    assert(readAnythingApp, 'no app');
    readAnythingApp.updateSelection();
  };

  chrome.readingMode.updateVoicePackStatus = (lang: string, status: string) => {
    const readAnythingApp = document.querySelector('read-anything-app');
    if (readAnythingApp) {
      readAnythingApp.updateVoicePackStatus(lang, status);
    }
  };

  chrome.readingMode.updateVoicePackStatusFromInstallResponse =
      (lang: string, status: string) => {
        const readAnythingApp = document.querySelector('read-anything-app');
        if (readAnythingApp) {
          readAnythingApp.updateVoicePackStatusFromInstallResponse(
              lang, status);
        }
      };

  chrome.readingMode.updateTheme = () => {
    const readAnythingApp = document.querySelector('read-anything-app');
    assert(readAnythingApp, 'no app');
    readAnythingApp.updateTheme();
  };

  chrome.readingMode.showLoading = () => {
    const readAnythingApp = document.querySelector('read-anything-app');
    assert(readAnythingApp, 'no app');
    readAnythingApp.showLoading();
  };

  chrome.readingMode.showEmpty = () => {
    const readAnythingApp = document.querySelector('read-anything-app');
    assert(readAnythingApp, 'no app');
    readAnythingApp.showEmpty();
  };

  chrome.readingMode.restoreSettingsFromPrefs = () => {
    const readAnythingApp = document.querySelector('read-anything-app');
    assert(readAnythingApp, 'no app');
    readAnythingApp.restoreSettingsFromPrefs();
  };

  chrome.readingMode.languageChanged = () => {
    const readAnythingApp = document.querySelector('read-anything-app');
    assert(readAnythingApp, 'no app');
    readAnythingApp.languageChanged();
  };

  chrome.readingMode.onLockScreen = () => {
    const readAnythingApp = document.querySelector('read-anything-app');
    assert(readAnythingApp, 'no app');
    readAnythingApp.onLockScreen();
  };
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
  // Did the user press play or pause
  paused: boolean;
  pauseSource?: PauseActionSource;
  // Has the user pressed `play` on this page.
  // TODO rename `speechStarted`, `paused` and `speechActuallyPlaying` to be
  // clearer.
  speechStarted: boolean;
  // Indicates that speech is currently playing. This differs from `paused`.
  // When a user presses the play button, paused becomes false, but
  // `speechActuallyPlaying` will tell us whether audio actually started playing
  // yet.
  speechActuallyPlaying: boolean;
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
    appFlexParent: HTMLElement,
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

  static get properties() {
    return {
      speechPlayingState: {
        type: Object,
        observer: 'onSpeechPlayingStateChanged_',
      },
    };
  }

  private startTime = Date.now();
  private constructorTime: number;

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override _attachDom(dom: Node|null): ShadowRoot|null {
    if (dom) {
      this.appendChild(dom);
    }
    return null;
  }

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
  private pendingImageRequest_?: PendingImageRequest;

  private scrollingOnSelection_: boolean;
  private hasContent_: boolean;
  private emptyStateImagePath_: string;
  private emptyStateDarkImagePath_: string;
  private emptyStateHeading_: string;
  private lastDownloadedLang_: string;
  private emptyStateSubheading_: string;

  private previousHighlight_: HTMLElement[] = [];
  private currentColorSuffix_: string;
  private isHighlightOn_: boolean = true;

  // If the WebUI toolbar should be shown. This happens when the WebUI feature
  // flag is enabled.
  private isWebUIToolbarVisible_: boolean;
  private isReadAloudEnabled_: boolean;

  // If the speech engine is considered "loaded." If it is, we should display
  // the play / pause buttons normally. Otherwise, we should disable the
  // Read Aloud controls until the engine has loaded in order to provide
  // visual feedback that a voice is about to be spoken.
  private speechEngineLoaded: boolean = true;

  // Sometimes distillations are queued up while distillation is happening so
  // when the current distillation finishes, we re-distill immediately. In that
  // case we shouldn't allow playing speech until the next distillation to avoid
  // resetting speech right after starting it.
  private willDrawAgainSoon_: boolean = false;

  // After the first utterance has been spoken, we should assume that the
  // speech engine has loaded, and we shouldn't adjust the play / pause
  // disabled state based on the message.onStart callback to avoid flickering.
  private firstUtteranceSpoken = false;

  synth = window.speechSynthesis;

  private selectedVoice: SpeechSynthesisVoice|undefined;
  private enabledLanguagesInPref: string[] = [];

  private availableVoices: SpeechSynthesisVoice[];
  // A set of availableLangs derived from availableVoices
  private availableLangs: string[] = [];
  // If a preview is playing, this is set to the voice the preview is playing.
  // Otherwise, this is undefined.
  private previewVoicePlaying: SpeechSynthesisVoice|null;

  private localeToDisplayName: {[locale: string]: string};

  private voicePackInstallStatus: {[language: string]: VoicePackStatus} = {};

  // Set of languages of the browser and/or of the pages navigated to that we
  // need to download Natural voices for automatically
  private languagesForVoiceDownloads: Set<string> = new Set();

  // Metrics captured for logging.
  private playSessionStartTime: number = -1;

  // State for speech synthesis paused/play state needs to be tracked explicitly
  // because there are bugs with window.speechSynthesis.paused and
  // window.speechSynthesis.speaking on some platforms.
  speechPlayingState: SpeechPlayingState = {
    paused: true,
    pauseSource: PauseActionSource.DEFAULT,
    speechStarted: false,
    speechActuallyPlaying: false,
  };

  maxSpeechLength = 175;

  wordBoundaryState: WordBoundaryState = {
    mode: WordBoundaryMode.BOUNDARIES_NOT_SUPPORTED,
    speechUtteranceStartIndex: 0,
    previouslySpokenIndex: 0,
  };

  // If the node id of the first text node that should be used by Read Aloud
  // has been set. This is null if the id has not been set.
  firstTextNodeSetForReadAloud: number|null = null;

  rate: number = 1;

  speechSynthesisLanguage: string;

  constructor() {
    super();
    this.constructorTime = Date.now();
    chrome.readingMode?.logMetric(
        (this.constructorTime - this.startTime),
        'Accessibility.ReadAnything.TimeFromAppStartedToConstructor');
    this.isReadAloudEnabled_ = chrome.readingMode.isReadAloudEnabled;
    this.isWebUIToolbarVisible_ = chrome.readingMode.isWebUIToolbarVisible;
    this.speechSynthesisLanguage = chrome.readingMode.baseLanguageForSpeech;
    if (chrome.readingMode && chrome.readingMode.isWebUIToolbarVisible) {
      ColorChangeUpdater.forDocument().start();
    }
  }

  override connectedCallback() {
    super.connectedCallback();
    // onConnected should always be called first in connectedCallback to ensure
    // we're not blocking onConnected on anything else during WebUI setup.
    if (chrome.readingMode) {
      chrome.readingMode.onConnected();
      const connectedCallbackTime = Date.now();
      chrome.readingMode.logMetric(
          (connectedCallbackTime - this.startTime),
          'Accessibility.ReadAnything.TimeFromAppStartedToConnectedCallback');
      chrome.readingMode.logMetric(
          (connectedCallbackTime - this.constructorTime),
          'Accessibility.ReadAnything.' +
              'TimeFromAppConstructorStartedToConnectedCallback');
    }

    // Wait until the side panel is fully rendered before showing the side
    // panel. This follows Side Panel best practices and prevents loading
    // artifacts from showing if the side panel is shown before content is
    // ready.
    listenOnce(this.$.appFlexParent, 'dom-change', () => {
      setTimeout(() => chrome.readingMode.shouldShowUi(), 0);
    });

    this.showLoading();

    if (this.isReadAloudEnabled_) {
      this.synth.onvoiceschanged = () => {
        this.getVoices(/*refresh =*/ true);
        // If the selected voice is now unavailable, such as after an install,
        // reselect a new voice.
        if (this.selectedVoice &&
            !this.availableVoices.some(
                voice => areVoicesEqual(voice, this.selectedVoice!))) {
          this.selectedVoice = undefined;
          this.getSpeechSynthesisVoice();
        }
      };
    }

    document.onselectionchange = () => {
      // When Read Aloud is playing, user-selection is disabled on the Read
      // Anything panel, so don't attempt to update selection, as this can
      // end up clearing selection in the main part of the browser.
      if (!this.hasContent_ || !this.speechPlayingState.paused) {
        return;
      }
      const selection = this.getSelection();
      assert(selection, 'no selection');
      const {anchorNode, anchorOffset, focusNode, focusOffset} = selection;
      if (!anchorNode || !focusNode) {
        // The selection was collapsed by clicking inside the selection.
        chrome.readingMode.onCollapseSelection();
        return;
      }
      let anchorNodeId = this.domNodeToAxNodeIdMap_.get(anchorNode);
      let focusNodeId = this.domNodeToAxNodeIdMap_.get(focusNode);
      let adjustedAnchorOffset = anchorOffset;
      let adjustedFocusOffset = focusOffset;
      // If the node was highlighted, then we need to find the parent node which
      // we stored in the map, rather than the node itself
      if (!anchorNodeId) {
        anchorNodeId = this.getHighlightedAncestorId_(anchorNode);
        adjustedAnchorOffset += this.getOffsetInAncestor(anchorNode);
      }
      if (!focusNodeId) {
        focusNodeId = this.getHighlightedAncestorId_(focusNode);
        adjustedFocusOffset += this.getOffsetInAncestor(focusNode);
      }
      if (!anchorNodeId || !focusNodeId) {
        return;
      }

      chrome.readingMode.onSelectionChange(
          anchorNodeId, adjustedAnchorOffset, focusNodeId, adjustedFocusOffset);
      // If there's been a selection, clear the current
      // Read Aloud highlight.
      const element = document.querySelector('.' + currentReadHighlightClass);
      if (element && anchorNodeId && focusNodeId) {
        element.classList.remove(currentReadHighlightClass);
      }

      // Clear the previously read highlight if there's been a selection.
      // If speech is resumed, this won't be restored.
      // TODO(b/40927698): Restore the previous highlight after speech
      // is resumed after a selection.
      this.previousHighlight_.forEach((element) => {
        if (element) {
          element.className = '';
        }
      });
      this.previousHighlight_ = [];

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
  }

  // Record metrics
  private recordPlayClick() {
    this.incrementMetricCount('Play');
  }

  private recordPauseClick() {
    this.incrementMetricCount('Pause');
  }

  private recordNextClick() {
    this.incrementMetricCount('NextButton');
  }

  private recordPreviousClick() {
    this.incrementMetricCount('PreviousButton');
  }

  private incrementMetricCount(action: string) {
    chrome.readingMode.incrementMetricCount(
        'Accessibility.ReadAnything.ReadAloud' + action + 'SessionCount');
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
    if (node.parentElement.className === parentOfHighlightClass) {
      ancestor = node.parentNode;
    } else if (
        node.parentElement.parentElement?.className ===
        parentOfHighlightClass) {
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

  // TODO(crbug.com/40910704): Potentially hide links during distillation.
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
    if (!this.firstTextNodeSetForReadAloud) {
      this.firstTextNodeSetForReadAloud = nodeId;
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

  private sendGetVoicePackInfoRequest(langOrLocale: string) {
    const langOrLocaleForPackManager =
        convertLangOrLocaleForVoicePackManager(langOrLocale);
    if (langOrLocaleForPackManager) {
      chrome.readingMode.sendGetVoicePackInfoRequest(
          langOrLocaleForPackManager);
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
    const selection = document.getSelection();
    return selection;
  }

  updateSelection() {
    const selection = this.getSelection()!;
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
    const selector = this.shouldShowLinks() ? 'span[data-link]' : 'a';
    const elements = this.querySelectorAll(selector);

    for (const elem of elements) {
      assert(elem instanceof HTMLElement, 'link is not an HTMLElement');
      const nodeId = this.domNodeToAxNodeIdMap_.get(elem);
      assert(nodeId !== undefined, 'link node id is undefined');
      const replacement = this.buildSubtree_(nodeId);
      this.replaceElement(elem, replacement);
    }
  }

  updateVoicePackStatusFromInstallResponse(lang: string, status: string) {
    if (!lang) {
      return;
    }

    const voicePackStatus = mojoVoicePackStatusToVoicePackStatusEnum(status);
    if (voicePackStatus === VoicePackStatus.INSTALL_ERROR) {
      // TODO (b/331795122) Handle install errors on the UI
      this.setVoicePackStatus_(lang, VoicePackStatus.INSTALL_ERROR);
      return;
    }


    // Do not rely on the status from Install response. It has responded
    // "installed" for voices that are not installed. Instead, request the
    // status from GetVoicePackInfo. The result will be returned in
    // updateVoicePackStatus().
    this.sendGetVoicePackInfoRequest(lang);
  }

  updateVoicePackStatus(lang: string, status: string) {
    if (!lang) {
      return;
    }

    const voicePackLangauge = this.getConvertedLangIfExists_(lang);

    const voicePackStatus = mojoVoicePackStatusToVoicePackStatusEnum(status);
    if (voicePackStatus === VoicePackStatus.EXISTS) {
      if (this.voicePackInstallStatus[voicePackLangauge] ===
          VoicePackStatus.DOWNLOADED) {
        // If the language pack is uninstalled but we still think it is
        // installed, then the user removed the language pack outside of reading
        // mode and we don't want to reinstall.
        this.setVoicePackStatus_(lang, VoicePackStatus.REMOVED_BY_USER);
      } else if (this.languagesForVoiceDownloads.has(lang)) {
        // We can't rely on the voice pack manager to reflect that a voice is
        // installing, so check our local state to see if we've already
        // triggered an install request.
        // Only call sendInstallVoicePackRequest() if it's not already
        // downloading
        if (this.getVoicePackStatus_(lang) !== VoicePackStatus.INSTALLING) {
          // TODO(b/326130935): Hide the message when installation completes or
          // show an error message if something fails.
          this.setVoicePackStatus_(lang, VoicePackStatus.INSTALLING);
          chrome.readingMode.sendInstallVoicePackRequest(lang);
        }
      }
    } else if (voicePackStatus === VoicePackStatus.DOWNLOADED) {
      // If we've never seen the voice pack for this language, then it was
      // already downloaded so mark it as such.
      if (!this.voicePackInstallStatus[voicePackLangauge]) {
        this.setVoicePackStatus_(voicePackLangauge, voicePackStatus);
      } else if (
          this.voicePackInstallStatus[voicePackLangauge] ===
          VoicePackStatus.INSTALLING) {
        const possibleLanguageConversion =
            convertLangToAnAvailableLangIfPresent(
                voicePackLangauge, this.availableLangs, true);
        this.lastDownloadedLang_ = possibleLanguageConversion ?
            possibleLanguageConversion :
            voicePackLangauge;
        this.showToast_();
        this.setVoicePackStatus_(voicePackLangauge, voicePackStatus);
      }

      // Force a refresh of the voices list since we might not get an update the
      // voices have changed.
      this.getVoices(true);
      return;
    } else {
      this.setVoicePackStatus_(lang, voicePackStatus);
      // TODO (b/335472298) Handle voice menu downloading voice spinners
    }
  }

  private getLanguageDownloadedTitle_(lang: string) {
    const langDisplayName =
        (this.localeToDisplayName && lang in this.localeToDisplayName) ?
        this.localeToDisplayName[lang] :
        lang;

    return loadTimeData.getStringF(
        'readingModeVoiceDownloadedTitle', langDisplayName);
  }

  // TODO(b/325962407): replace toast with system notification
  private showToast_(): void {
    if (!this.shadowRoot) {
      return;
    }

    const toast = this.shadowRoot.querySelector<CrToastElement>('#toast')!;
    toast.show();
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
    // TODO(crbug.com/40927698): Additional logic to find default voice if there
    // isn't a voice marked as default
    const baseLang = this.speechSynthesisLanguage;
    const allPossibleVoices = this.getVoices();
    const voicesForLanguage =
        allPossibleVoices.filter(voice => voice.lang.startsWith(baseLang));

    if (!voicesForLanguage || (voicesForLanguage.length === 0)) {
      // Stay with the current voice if no voices are available for this
      // language. If no voice is yet selected, use the default voice.
      return this.selectedVoice ?
          this.selectedVoice :
          this.getVoices().find(({default: isDefaultVoice}) => isDefaultVoice);
    }

    // First try to choose a voice only from currently enabled locales for this
    // language.
    const voicesForEnabledLocale = voicesForLanguage.filter(
        voice =>
            this.enabledLanguagesInPref.includes(voice.lang.toLowerCase()));
    if (!voicesForEnabledLocale || voicesForEnabledLocale.length === 0) {
      // If there are no enabled locales for this language, choose a voice
      // labeled default among all enabled voices, or the first enabled voice.
      const allVoicesForEnabledLocales = allPossibleVoices.filter(
          v => this.enabledLanguagesInPref.includes(v.lang.toLowerCase()));
      if (!allVoicesForEnabledLocales) {
        // If there are no enabled locales, we can't select a voice. So return
        // undefined so we can disable the play button.
        return undefined;
      } else {
        const defaultVoiceForEnabledLocales = allVoicesForEnabledLocales.find(
            ({default: isDefaultVoice}) => isDefaultVoice);
        return defaultVoiceForEnabledLocales ? defaultVoiceForEnabledLocales :
                                               allVoicesForEnabledLocales[0];
      }
    }

    // Get the default voice for the currently enabled locale.
    const defaultVoiceForLanguage = voicesForEnabledLocale.find(
        ({default: isDefaultVoice}) => isDefaultVoice);

    return defaultVoiceForLanguage ? defaultVoiceForLanguage :
                                     voicesForEnabledLocale[0];
  }

  // Attempt to get a new voice using the current language. In theory, the
  // previously unavailable voice should no longer be showing up in
  // getVoices, but we ensure that the alternative voice does not match
  // the previously unavailable voice as an extra measure. This method should
  // only be called when speech synthesis returns an error.
  getAlternativeVoice(unavailableVoice: SpeechSynthesisVoice|
                      null): SpeechSynthesisVoice|null {
    const newVoice = this.defaultVoice();

    // If the default voice is not the same as the original, unavailable voice,
    // use that, only if the new voice is also defined.
    if (newVoice !== undefined && !areVoicesEqual(newVoice, unavailableVoice)) {
      return newVoice;
    }

    // If the default voice won't work, try another voice in that language.
    const baseLang = this.speechSynthesisLanguage;
    const voicesForLanguage =
        this.getVoices().filter(voice => voice.lang.startsWith(baseLang));

    // TODO(b/40927698): It's possible we can get stuck in an infinite loop
    // of jumping back and forth between two or more invalid voices, if
    // multiple voices are invalid. Investigate if we need to do more to handle
    // this case.

    // TODO(b/336596926): If there still aren't voices for the language,
    // attempt to fallback to the browser language, if we're using the page
    // language.
    if (!voicesForLanguage || (voicesForLanguage.length === 0)) {
      return null;
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
    return null;
  }

  private getVoices(refresh: boolean = false): SpeechSynthesisVoice[] {
    if (!this.availableVoices || refresh) {
      let availableVoices = this.synth.getVoices();
      if (availableVoices.some(({localService}) => localService)) {
        availableVoices =
            availableVoices.filter(({localService}) => localService);
      }
      // Filter out Android voices on ChromeOS. Android Speech Recognition
      // voices are technically network voices, but for some reason, some
      // voices are marked as localService voices, so filtering localService
      // doesn't filter them out. Since they can cause unexpected behavior
      // in Read Aloud, go ahead and filter them out. To avoid causing any
      // unexpected behavior outside of ChromeOS, just filter them on ChromeOS.
      if (chrome.readingMode.isChromeOsAsh) {
        availableVoices = availableVoices.filter(
            ({name}) => !name.toLowerCase().includes('android'));
      }
      this.availableVoices = availableVoices;
      this.availableLangs = [...new Set(availableVoices.map(({lang}) => lang))];

      this.populateDisplayNamesForLocaleCodes();

      // Update voice pack install status if we're refreshing the list.
      if (refresh) {
        this.availableLangs
            .filter(
                lang =>
                    this.voicePackInstallStatus[this.getConvertedLangIfExists_(
                        lang)] === VoicePackStatus.DOWNLOADED)
            .forEach(downloadedLang => {
              this.setVoicePackStatus_(
                  downloadedLang, VoicePackStatus.INSTALLED);
            });
      }
    }
    return this.availableVoices;
  }

  private populateDisplayNamesForLocaleCodes() {
    this.localeToDisplayName = {};

    // Get display names for all the pack manager supported locales, only on
    // ChromeOS.
    if (chrome.readingMode.isChromeOsAsh) {
      AVAILABLE_GOOGLE_TTS_LOCALES.forEach((lang) => {
        this.maybeAddDisplayName(lang);
      });
    }

    // Get any remaining display names for languages of available voices.
    for (const {lang} of this.availableVoices) {
      this.maybeAddDisplayName(lang);
    }
  }

  private maybeAddDisplayName(lang: string) {
    if (!(lang in this.localeToDisplayName)) {
      const langDisplayName =
          chrome.readingMode.getDisplayNameForLocale(lang, lang);
      if (langDisplayName) {
        this.localeToDisplayName =
            {...this.localeToDisplayName, [lang]: langDisplayName};
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

  private onPreviewVoice_(
      event: CustomEvent<{previewVoice: SpeechSynthesisVoice}>) {
    event.preventDefault();
    event.stopPropagation();

    this.stopSpeech(PauseActionSource.VOICE_PREVIEW);

    // If there's no previewVoice, return after stopping the current preview
    if (!event.detail) {
      this.previewVoicePlaying = null;
      return;
    }

    const defaultUtteranceSettings = this.defaultUtteranceSettings();
    // TODO(crbug.com/40927698): Finalize the default voice preview text.
    const utterance = new SpeechSynthesisUtterance(
        loadTimeData.getString('readingModeVoicePreviewText'));
    const voice = event.detail.previewVoice;
    utterance.voice = voice;
    utterance.lang = defaultUtteranceSettings.lang;
    utterance.volume = defaultUtteranceSettings.volume;
    utterance.pitch = defaultUtteranceSettings.pitch;
    utterance.rate = defaultUtteranceSettings.rate;

    // TODO(crbug.com/40927698): Add tests for pause button
    utterance.onstart = event => {
      this.previewVoicePlaying = event.utterance.voice;
    };

    utterance.onend = () => {
      this.previewVoicePlaying = null;
    };

    // TODO(b/40927698): There should probably be more sophisticated error
    // handling for voice previews, but for now, simply setting the preview
    // voice to null should be sufficient to reset state if an error is
    // encountered during a preview.
    utterance.onerror = () => {
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
      this.playSessionStartTime = Date.now();
      this.recordPlayClick();
      this.playSpeech();
    } else {
      this.logSpeechPlaySession();
      this.recordPauseClick();
      this.stopSpeech(PauseActionSource.BUTTON_CLICK);
    }
  }

  private onSpeechPlayingStateChanged_() {
    chrome.readingMode.onSpeechPlayingStateChanged(
        this.speechPlayingState.paused);
  }

  stopSpeech(pauseSource: PauseActionSource) {
    this.speechPlayingState = {
      ...this.speechPlayingState,
      paused: true,
      speechActuallyPlaying: false,
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

  private logSpeechPlaySession() {
    // Don't log a playback session just in case something has gotten out of
    // sync and we call stopSpeech before playSpeech.
    assert(this.playSessionStartTime > 0, 'invalid speech playing start time');
    if (this.playSessionStartTime > 0) {
      chrome.readingMode.logLongMetric(
          Date.now() - this.playSessionStartTime,
          'Accessibility.ReadAnything.SpeechPlaybackSession');
      this.playSessionStartTime = -1;
    }
  }

  private playNextGranularity_() {
    this.recordNextClick();
    this.synth.cancel();
    this.resetPreviousHighlight();
    // Reset the word boundary index whenever we move the granularity position.
    this.resetToDefaultWordBoundaryState();
    chrome.readingMode.movePositionToNextGranularity();

    if (!this.highlightAndPlayMessage()) {
      this.onSpeechFinished();
    }
  }

  private playPreviousGranularity_() {
    this.recordPreviousClick();
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

      this.speechPlayingState = {
        paused: false,
        speechStarted: true,
        speechActuallyPlaying: this.speechPlayingState.speechActuallyPlaying,
      };

      // Hide links when speech resumes. We only hide links when the page was
      // paused from the play/pause button.
      if (chrome.readingMode.linksEnabled && pausedFromButton) {
        this.updateLinks();
        // Now that links are toggled, ensure that the new nodes are also
        // highlighted.
        this.highlightNodes(chrome.readingMode.getCurrentText());
      }

      // If the current read highlight has been cleared from a call to
      // updateContent, such as for links being toggled on or off via a Read
      // Aloud play / pause or via a preference change, rehighlight the nodes
      // after a pause.
      if (!container.querySelector('.' + currentReadHighlightClass)) {
        this.highlightNodes(chrome.readingMode.getCurrentText());
      }

      return;
    }
    if (container.textContent) {
      this.speechPlayingState = {
        paused: false,
        speechStarted: true,
        speechActuallyPlaying: this.speechPlayingState.speechActuallyPlaying,
      };
      // Hide links when speech begins playing.
      if (chrome.readingMode.linksEnabled) {
        this.updateLinks();
      }

      // TODO(crbug.com/40927698): There should be a way to use AXPosition so
      // that this step can be skipped.
      if (this.firstTextNodeSetForReadAloud) {
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
      return this.highlightAndPlayMessage();
    }

    this.playText(utteranceText);
    if (this.wordBoundaryState.mode ===
            WordBoundaryMode.BOUNDARIES_NOT_SUPPORTED ||
        !this.shouldUseWordHighlighting()) {
      this.highlightNodes(axNodeIds);
    } else {
      this.highlightNodesForWordBoundary();
    }
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
    const isTextTooLong = this.selectedVoice?.localService ?
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
      this.speechEngineLoaded = true;

      if (error.error === 'interrupted') {
        // SpeechSynthesis.cancel() was called, therefore, do nothing.
        return;
      }

      // Log a speech error. We aren't concerned with logging an interrupted
      // error, since that can be triggered from play / pause.
      chrome.readingMode.logSpeechError(error.error);

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
        this.updateSpeechRate_(1);
      }

      // No appropriate voice is available for the language designated in
      // SpeechSynthesisUtterance lang.
      if (error.error === 'language-unavailable') {
        const possibleNewLanguage = convertLangToAnAvailableLangIfPresent(
            this.speechSynthesisLanguage, this.availableLangs,
            /* allowCurrentLanguageIfExists */ false);
        if (possibleNewLanguage) {
          this.speechSynthesisLanguage = possibleNewLanguage;
        }
      }

      // The voice designated in SpeechSynthesisUtterance voice attribute
      // is not available.
      if (error.error === 'voice-unavailable') {
        let newVoice = this.selectedVoice ? this.selectedVoice : null;
        this.selectedVoice = undefined;
        newVoice = this.getAlternativeVoice(newVoice);

        if (newVoice) {
          this.selectedVoice = newVoice;
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
      // the sentence granularity level, so we'll retrieve these boundaries in message.onEnd
      // instead.
      if (event.name === 'word') {
        this.updateBoundary(event.charIndex);

        // Only update the highlighting with word highlights if they should be
        // used.
        if (this.shouldUseWordHighlighting()) {
          this.highlightNodesForWordBoundary();
        }
      }
    });

    message.onstart = () => {
      // We've gotten the signal that the speech engine has loaded, therefore
      // we can enable the Read Aloud buttons.
      this.speechEngineLoaded = true;

      if (!this.speechPlayingState.speechActuallyPlaying) {
        this.speechPlayingState = {
          ...this.speechPlayingState,
          speechActuallyPlaying: true,
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
      // TODO(crbug.com/40927698): Handle already selected text.
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


    if (!this.firstUtteranceSpoken) {
      this.speechEngineLoaded = false;
      this.firstUtteranceSpoken = true;
    }
    this.synth.speak(message);
  }

  updateBoundary(charIndex: number) {
    this.wordBoundaryState.previouslySpokenIndex = charIndex;
    this.wordBoundaryState.mode = WordBoundaryMode.BOUNDARY_DETECTED;
  }

  resetToDefaultWordBoundaryState() {
    this.wordBoundaryState = {
      previouslySpokenIndex: 0,
      mode: this.wordBoundaryState.mode === WordBoundaryMode.BOUNDARY_DETECTED ?
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

  // TODO(b/301131238): Verify all edge cases.
  highlightNodesForWordBoundary() {
    const index = this.wordBoundaryState.speechUtteranceStartIndex +
        this.wordBoundaryState.previouslySpokenIndex;
    const highlightNode: number =
        chrome.readingMode.getNodeIdForCurrentSegmentIndex(index);
    const highlightLength: number =
        chrome.readingMode.getNextWordHighlightLength(index);
    const element = this.domNodeToAxNodeIdMap_.keyFrom(highlightNode);
    const startIndex =
        chrome.readingMode.getCurrentTextStartIndex(highlightNode) + index;
    const endIndex = startIndex + highlightLength;
    if (!element ||
        element.textContent?.substring(startIndex, endIndex).trim() === '') {
      return;
    }
    this.highlightCurrentText_(startIndex, endIndex, element as HTMLElement);
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
      this.highlightCurrentText_(start, end, element as HTMLElement);
    }
  }

  private defaultUtteranceSettings(): UtteranceSettings {
    const lang = this.speechSynthesisLanguage;

    return {
      lang,
      // TODO(crbug.com/40927698): Ensure the rate is valid for the current
      // speech engine.
      rate: this.rate,
      // TODO(crbug.com/40927698): Ensure the correct default values are used.
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
    parentOfHighlight.className = parentOfHighlightClass;

    // First pull out any text within this node before the highlighted section.
    // Since it's already been highlighted, we fade it out.
    const highlightPrefix =
        currentNode.textContent!.substring(0, highlightStart);
    if (highlightPrefix.length > 0) {
      const prefixNode = document.createElement('span');
      prefixNode.className = previousReadHighlightClass;
      prefixNode.textContent = highlightPrefix;
      parentOfHighlight.appendChild(prefixNode);
    }

    // Then get the section of text to highlight and mark it for
    // highlighting.
    const readingHighlight = document.createElement('span');
    readingHighlight.className = currentReadHighlightClass;
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
    this.previousHighlight_.push(readingHighlight);
    this.replaceElement(currentNode, parentOfHighlight);

    // Automatically scroll the text so the highlight stays roughly centered.
    readingHighlight.scrollIntoViewIfNeeded();
  }

  private onSpeechFinished() {
    this.clearReadAloudState();

    // Show links when speech finishes playing.
    if (chrome.readingMode.linksEnabled) {
      this.updateLinks();
    }
    // Clear the formatting we added for highlighting.
    this.updateContent();
    this.logSpeechPlaySession();
  }

  private clearReadAloudState() {
    this.speechPlayingState = {
      paused: true,
      pauseSource: PauseActionSource.DEFAULT,
      speechStarted: false,
      speechActuallyPlaying: false,
    };
    this.previousHighlight_ = [];
    this.resetToDefaultWordBoundaryState();
  }

  private shouldUseWordHighlighting(): boolean {
    // Word highlighting should only be used for speech rates less than or
    // equal to 1x speed.
    return chrome.readingMode.isAutomaticWordHighlightingEnabled &&
        this.rate <= 1;
  }

  private onSelectVoice_(
      event: CustomEvent<{selectedVoice: SpeechSynthesisVoice}>) {
    event.preventDefault();
    event.stopPropagation();

    this.selectedVoice = event.detail.selectedVoice;
    chrome.readingMode.onVoiceChange(
        this.selectedVoice.name, this.selectedVoice.lang);

    this.resetSpeechPostSettingChange_();
  }

  private onVoiceLanguageToggle_(event: CustomEvent<{language: string}>) {
    event.preventDefault();
    event.stopPropagation();
    const toggledLanguage = event.detail.language;
    const currentlyEnabled =
        this.enabledLanguagesInPref.includes(toggledLanguage);

    if (!currentlyEnabled) {
      this.installVoicePackIfPossible(toggledLanguage);
    }
    this.enabledLanguagesInPref = currentlyEnabled ?
        this.enabledLanguagesInPref.filter(lang => lang !== toggledLanguage) :
        [...this.enabledLanguagesInPref, toggledLanguage];

    chrome.readingMode.onLanguagePrefChange(toggledLanguage, !currentlyEnabled);

    if (!currentlyEnabled && !this.selectedVoice) {
      // If there were no enabled languages (and thus no selected voice), select
      // a voice.
      this.getSpeechSynthesisVoice();
    }
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

  // TODO(crbug.com/40275871): This method should be renamed to
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

  // This must be called BEFORE calling
  // chrome.readingMode.movePositionToPreviousGranularity so we can accurately
  // determine what's currently being highlighted.
  private resetPreviousHighlightAndRemoveCurrentHighlight() {
    // The most recent highlight could have been spread across multiple segments
    // so clear the formatting for all of the segments.
    for (let i = 0; i < chrome.readingMode.getCurrentText().length; i++) {
      const lastElement = this.previousHighlight_.pop();
      if (lastElement) {
        lastElement.className = '';
      }
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
      this.selectPreferredVoice_();
      this.restoreEnabledLanguagesFromPref_();
    }
    this.updateLineSpacing_(chrome.readingMode.lineSpacing);
    this.updateLetterSpacing_(chrome.readingMode.letterSpacing);
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
      this.updateThemeFromWebUi_(colorSuffix);
    }
    // TODO(crbug.com/40927698): investigate using parent/child relationshiop
    // instead of element by id.
    this.$.toolbar.restoreSettingsFromPrefs(colorSuffix);
  }

  private restoreEnabledLanguagesFromPref_() {
    // We need to make sure the languages we choose correspond to voices, so
    // refresh the list of voices and available langs
    this.getVoices();

    const storedLanguagesPref: string[] =
        chrome.readingMode.getLanguagesEnabledInPref();
    const browserOrPageBaseLang = chrome.readingMode.baseLanguageForSpeech;
    this.speechSynthesisLanguage = browserOrPageBaseLang;

    this.enabledLanguagesInPref = createInitialListOfEnabledLanguages(
        browserOrPageBaseLang, storedLanguagesPref, this.availableLangs,
        this.defaultVoice()?.lang);

    storedLanguagesPref.forEach(storedLanguage => {
      if (!this.enabledLanguagesInPref.find(
              language => language === storedLanguage)) {
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

    for (const lang of this.enabledLanguagesInPref) {
      this.installVoicePackIfPossible(lang);
    }
  }

  private selectPreferredVoice_() {
    // TODO: b/40275871 - decide whether this is the behavior we want. This
    // shouldn't happen often, so just skip selecting a new voice for now.
    // Another option would be to update the voice and the call
    // resetSpeechPostSettingsChange(), but that could be jarring.
    if (this.speechPlayingState.speechStarted) {
      return;
    }

    const storedVoiceName = chrome.readingMode.getStoredVoice();
    if (!storedVoiceName) {
      this.selectedVoice = this.defaultVoice();
      return;
    }

    const selectedVoice = this.getVoices()
                              .filter(voice => voice.name === storedVoiceName);

    this.selectedVoice = selectedVoice ? selectedVoice[0] : this.defaultVoice();
  }

  private onLineSpacingChange_(event: CustomEvent<{data: number}>) {
    this.updateLineSpacing_(event.detail.data);
  }

  private updateLineSpacing_(newLineHeight: number) {
    this.updateStyles({
      '--line-height': newLineHeight,
    });
  }

  private onLetterSpacingChange_(event: CustomEvent<{data: number}>) {
    this.updateLetterSpacing_(event.detail.data);
  }

  private updateLetterSpacing_(newLetterSpacing: number) {
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
    this.isHighlightOn_ = event.detail.highlightOn;
    this.updateStyles({
      '--current-highlight-bg-color':
          this.getCurrentHighlightColorVar(this.currentColorSuffix_),
    });
  }

  private onThemeChange_(event: CustomEvent<{data: string}>) {
    this.updateThemeFromWebUi_(event.detail.data);
  }

  private onResetToolbar_() {
    this.updateStyles({
      '--app-overflow-x': overflowXTypical,
      '--container-min-width': minWidthTypical,
    });
  }

  private onToolbarOverflow_(event: CustomEvent<{overflowLength: number}>) {
    const shouldScroll =
        (event.detail.overflowLength >= minOverflowLengthToScroll);
    this.updateStyles({
      '--app-overflow-x': shouldScroll ? overflowXScroll : overflowXTypical,
      // When we scroll, we should allow the container to expand and scroll
      // horizontally.
      '--container-min-width': shouldScroll ? minWidthOverflow :
                                              minWidthTypical,
    });
  }

  // TODO(crbug.com/40275871): This method should be renamed to updateTheme()
  // and replace the one below once we've removed the Views toolbar.
  private updateThemeFromWebUi_(colorSuffix: string) {
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
    if (!this.isHighlightOn_) {
      return 'transparent';
    }
    if (colorSuffix === '') {
      return 'var(--color-text-selection-background)';
    }
    return `var(--color-read-anything-current-read-aloud-highlight${
        colorSuffix})`;
  }

  getPreviousHighlightColorVar(colorSuffix: string) {
    if (colorSuffix === '') {
      return 'var(--color-sys-on-surface-subtle)';
    }
    return `var(--color-read-anything-previous-read-aloud-highlight${
        colorSuffix})`;
  }

  getBackgroundColorVar(colorSuffix: string) {
    if (colorSuffix === '') {
      return 'var(--color-sys-base-container-elevated)';
    }
    return `var(--color-read-anything-background${colorSuffix})`;
  }

  getForegroundColorVar(colorSuffix: string) {
    if (colorSuffix === '') {
      return 'var(--color-sys-on-surface)';
    }
    return `var(--color-read-anything-foreground${colorSuffix})`;
  }

  getSelectionColorVar(colorSuffix: string) {
    if (colorSuffix === '') {
      return 'var(--color-text-selection-background)';
    }
    return `var(--color-read-anything-text-selection${colorSuffix})`;
  }

  getSelectionTextColorVar(colorSuffix: string) {
    if (colorSuffix === '') {
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

  // If the screen is locked during speech, we should stop speaking.
  onLockScreen() {
    if (!this.speechPlayingState.paused) {
      this.stopSpeech(PauseActionSource.DEFAULT);
    }
  }

  languageChanged() {
    this.speechSynthesisLanguage = chrome.readingMode.baseLanguageForSpeech;
    this.$.toolbar.updateFonts();
    if (chrome.readingMode.isAutoVoiceSwitchingEnabled) {
      this.selectPreferredVoice_();
    }
    this.installVoicePackIfPossible(this.speechSynthesisLanguage);
  }

  // Include parameters in order to force a re-render whenever the values
  // change.
  isReadAloudPlayable(
      hasContent: boolean = this.hasContent_,
      selectedVoice: SpeechSynthesisVoice|undefined = this.selectedVoice,
      speechEngineLoaded: boolean = this.speechEngineLoaded,
      willDrawAgainSoon: boolean = this.willDrawAgainSoon_): boolean {
    return hasContent && speechEngineLoaded && (selectedVoice !== undefined) &&
        !willDrawAgainSoon;
  }

  // Kicks off a workflow to install a voice pack.
  // 1) Checks if Language Pack Manager supports a version of this voice/locale
  // 2) If so, adds voice to installVoicePackIfPossible set
  // 3) Kicks off request GetVoicePackInfo to see if the voice is installed
  // 4) Upon response, if we see the voice is not installed and that it's in
  // installVoicePackIfPossible, then we trigger an install request
  private installVoicePackIfPossible(langOrLocale: string) {
    if (!chrome.readingMode.isLanguagePackDownloadingEnabled) {
      return;
    }

    const langCodeForVoicePackManager =
        convertLangOrLocaleForVoicePackManager(langOrLocale);

    if (!langCodeForVoicePackManager) {
      this.setVoicePackStatus_(langOrLocale, VoicePackStatus.NONE);
      return;
    }

    const statusForLang =
        this.voicePackInstallStatus[langCodeForVoicePackManager];
    if (!statusForLang || (statusForLang === VoicePackStatus.EXISTS)) {
      this.languagesForVoiceDownloads.add(langCodeForVoicePackManager);
      // Inquire if the voice pack is downloaded. If not, it'll trigger a
      // download when we get the response in updateVoicePackStatus().
      this.sendGetVoicePackInfoRequest(langCodeForVoicePackManager);
      this.setVoicePackStatus_(
          langCodeForVoicePackManager, VoicePackStatus.EXISTS);
    } else if (statusForLang === VoicePackStatus.DOWNLOADED) {
      // Force a refresh of the voices list since we might not get an update the
      // voices have changed.
      this.getVoices(/*refresh=*/ true);
    }
  }

  private onKeyDown_(e: KeyboardEvent) {
    if (e.key === 'k') {
      e.stopPropagation();
      this.onPlayPauseClick_();
    }
  }

  private getVoicePackStatus_(lang: string): VoicePackStatus|undefined {
    const voicePackLanguage = this.getConvertedLangIfExists_(lang);
    return this.voicePackInstallStatus[voicePackLanguage];
  }

  private setVoicePackStatus_(lang: string, status: VoicePackStatus) {
    // Convert the language string to ensure consistency across
    // languages and locales when setting the status.
    const voicePackLanguage = this.getConvertedLangIfExists_(lang);

    this.voicePackInstallStatus = {
      ...this.voicePackInstallStatus,
      [voicePackLanguage]: status,
    };
  }

  private getConvertedLangIfExists_(lang: string): string {
    const voicePackLanguage = convertLangOrLocaleForVoicePackManager(lang);

    // If the voice pack language wasn't converted, use the original string.
    // This will enable us to set install statuses on invalid languages and
    // locales.
    if (!voicePackLanguage) {
      return lang;
    }

    return voicePackLanguage;
  }
}


declare global {
  interface HTMLElementTagNameMap {
    'read-anything-app': ReadAnythingElement;
  }
}

customElements.define(ReadAnythingElement.is, ReadAnythingElement);
