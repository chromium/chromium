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
import {minOverflowLengthToScroll} from './common.js';
import type {LanguageToastElement} from './language_toast.js';
import {NodeStore} from './node_store.js';
import {SpeechController} from './read_aloud/speech_controller.js';
import type {SpeechListener} from './read_aloud/speech_controller.js';
import {VoiceLanguageController} from './read_aloud/voice_language_controller.js';
import type {VoiceLanguageListener} from './read_aloud/voice_language_controller.js';
import {ReadAnythingLogger, TimeFrom} from './read_anything_logger.js';
import type {ReadAnythingToolbarElement} from './read_anything_toolbar.js';
import {VoiceNotificationManager} from './voice_notification_manager.js';

const AppElementBase = WebUiListenerMixinLit(CrLitElement);

const linkDataAttribute = 'link';

export interface AppElement {
  $: {
    toolbar: ReadAnythingToolbarElement,
    appFlexParent: HTMLElement,
    container: HTMLElement,
    containerParent: HTMLElement,
    languageToast: LanguageToastElement,
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

  constructor() {
    super();
    this.constructorTime = Date.now();
    this.logger_.logTimeFrom(
        TimeFrom.APP, this.startTime, this.constructorTime);
    this.isReadAloudEnabled_ = chrome.readingMode.isReadAloudEnabled;
    this.styleUpdater_ = new AppStyleUpdater(this);
    this.nodeStore_.clear();
    ColorChangeUpdater.forDocument().start();
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
      const selection: Selection = this.getSelection();
      assert(selection, 'no selection');
      if (!selection.anchorNode || !selection.focusNode) {
        // The selection was collapsed by clicking inside the selection.
        chrome.readingMode.onCollapseSelection();
        return;
      }

      const {anchorNodeId, anchorOffset, focusNodeId, focusOffset} =
          this.isReadAloudEnabled_ ?
          this.speechController_.getSelectionAdjustedForHighlights(
              selection.anchorNode, selection.anchorOffset, selection.focusNode,
              selection.focusOffset) :
          this.getSelection();
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

      if (this.isReadAloudEnabled_) {
        this.speechController_.onSelectionChange();
      }
    };

    this.$.containerParent.onscroll = () => {
      chrome.readingMode.onScroll(this.scrollingOnSelection_);
      this.scrollingOnSelection_ = false;
      if (this.isReadAloudEnabled_) {
        this.speechController_.onScroll();
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
    if (this.isReadAloudEnabled_) {
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
    if (this.isReadAloudEnabled_) {
      this.speechController_.clearReadAloudState();
    }
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
      this.speechController_.clearReadAloudState();
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
        chrome.readingMode.onNoTextContent(/* previouslyHadContent*/ true);
      } else if (!this.isEmptyState()) {
        // If no text content is found but reading mode is not showing the
        // empty state, signal back to the renderer that this is the case.
        // This is possible when the AXTree returns bad selection data and
        // reading mode believes it has selected content to distll but
        // nothing valid is selected. This can cause the loading screen
        // to never switch to the empty state.
        // TODO: crbug.com/411198154- Longer term, once reading mode and read
        // aloud traversal is more in line, the renderer should be able to call
        // showEmpty directly, rather than signaling to the WebUI to update
        // content and then WebUI signaling back to the renderer that there is
        // no text content.
        chrome.readingMode.onNoTextContent(/* previouslyHadContent*/ false);
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
    if (this.isReadAloudEnabled_) {
      this.speechController_.setPreviousReadingPositionIfExists();
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

  protected updateLinks_() {
    if (!this.shadowRoot) {
      return;
    }

    const selector = this.shouldShowLinks() ? 'span[data-link]' : 'a';
    const elements = this.shadowRoot.querySelectorAll(selector);

    for (const elem of elements) {
      assert(elem instanceof HTMLElement, 'link is not an HTMLElement');
      const nodeId = this.nodeStore_.getAxId(elem);
      assert(nodeId !== undefined, 'link node id is undefined');
      const replacement = this.buildSubtree_(nodeId);
      this.nodeStore_.replaceDomNode(elem, replacement);
    }

    if (this.isReadAloudEnabled_) {
      this.speechController_.onLinksToggled();
    }
    this.loadImages_();
  }

  protected updateImages_() {
    if (!this.shadowRoot || !chrome.readingMode.imagesFeatureEnabled) {
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
        this.getSelection(), this.$.container.textContent);
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
      chrome.readingMode.onNoTextContent(/*previouslyHadContent*/ true);
    }
  }

  languageChanged() {
    this.$.toolbar.updateFonts();
    if (this.isReadAloudEnabled_) {
      this.voiceLanguageController_.onPageLanguageChanged();
    }
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
