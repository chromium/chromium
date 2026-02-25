// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';

import {previousReadHighlightClass} from '../read_aloud/movement.js';
import {getReadAloudModel} from '../read_aloud/read_aloud_model_browser_proxy.js';
import {ReadAloudNode} from '../read_aloud/read_aloud_types.js';
import {SpeechController} from '../read_aloud/speech_controller.js';
import {isDistilledByReadability, LOG_EMPTY_DELAY_MS} from '../shared/common.js';
import {ReadAnythingLogger} from '../shared/read_anything_logger.js';

import {NodeStore} from './node_store.js';
import {ReadabilityImageClassifier} from './readability_image_classifier.js';

const DATA_PREFIX = 'data-';
const LINK_DATA_ATTR = 'link';
const LINKS_OFF_TAG = 'span';
const LINKS_ON_TAG = 'a';
const LINKS_OFF_SELECTOR =
    LINKS_OFF_TAG + '[' + DATA_PREFIX + LINK_DATA_ATTR + ']';
export const HIGHLIGHTED_LINK_CLASS = 'highlighted-link';

// Reading mode sometimes needs to use a different html tag to display a
// particular node than the one used in the main panel. This maps the tags
// received from the renderer to the tag to use in Reading mode.
const SCREEN2X_TAG_TO_RM_TAG: Map<string, string> = new Map([
  // getHtmlTag might return '#document' which is not a valid to pass to
  // createElement.
  ['#document', 'div'],
  // Only one body tag is allowed per document.
  ['body', 'div'],
  // details tags hide content beneath them if closed. If opened, there is
  // content underneath we should show, but surrounding it with a generic
  // details tag causes it to be hidden in reading mode. So use a div instead.
  // In the cases that the details are closed, then nothing will be returned
  // beneath the details tag so nothing is rendered on reading mode.
  ['details', 'div'],
  ['img', 'canvas'],
  // Sometimes videos are marked with the role of image, especially if they're
  // gifs. Draw a still image on the canvas instead of a moving image.
  // TODO(crbug.com/439634112): Consider a setting to allow moving images.
  ['video', 'canvas'],
  // Buttons are sometimes distilled but button click logic isn't handled
  // by reading mode, so these shouldn't be distilled as clickable elements.
  ['button', 'div'],
  // Reading mode shouldn't be distilling input elements, but if this ever
  // happens, ensure they're converted to divs so that an interactable
  // element isn't shown on the reading mode panel.
  ['input', 'div'],
]);

// Readability doesn't need to replace as many tags as Screen2x. If there's
// more overlap in the future, the Screen2x map and the Readability map
// may need to be merged more.
const READABILITY_TAG_TO_RM_TAG: Map<string, string> = new Map([
  ['button', 'div'],
  ['details', 'div'],
  ['mark', 'div'],
]);

export interface ContentListener {
  // Called when the current state changes between the different ContentTypes.
  onContentStateChange(): void;
  // Called when the new content is determined to be a different page from
  // before.
  onNewPageDrawn(): void;
  // Called when any content on the page has changed. e.g. content is added or
  // removed.
  onContentChange(): void;
}

export enum ContentType {
  // Reading mode is loading and may or may not have content when it finishes.
  LOADING,
  // There is no content to display but the user can try to select text to get
  // content.
  NO_CONTENT,
  // There is no content to display and the user cannot select text to get
  // content.
  NO_SELECTABLE_CONTENT,
  // There is content displayed in Reading mode.
  HAS_CONTENT,
}

export interface ContentState {
  type: ContentType;
  imagePath: string;
  darkImagePath: string;
  heading: string;
  subheading: string;
}

// Use a Record to enforce that every ContentType has a corresponding
// ContentState.
const CONTENT_STATES: Record<ContentType, ContentState> = {
  [ContentType.LOADING]: {
    type: ContentType.LOADING,
    imagePath: '//resources/images/throbber_small.svg',
    darkImagePath: '//resources/images/throbber_small_dark.svg',
    heading: loadTimeData.getString('readAnythingLoadingMessage'),
    subheading: '',
  },
  [ContentType.NO_CONTENT]: {
    type: ContentType.NO_CONTENT,
    imagePath: './images/empty_state.svg',
    darkImagePath: './images/empty_state.svg',
    heading: loadTimeData.getString('emptyStateHeader'),
    subheading: loadTimeData.getString('emptyStateSubheader'),
  },
  [ContentType.NO_SELECTABLE_CONTENT]: {
    type: ContentType.NO_SELECTABLE_CONTENT,
    imagePath: './images/empty_state.svg',
    darkImagePath: './images/empty_state.svg',
    heading: loadTimeData.getString('notSelectableHeader'),
    subheading: loadTimeData.getString('emptyStateSubheader'),
  },
  [ContentType.HAS_CONTENT]: {
    type: ContentType.HAS_CONTENT,
    imagePath: '',
    darkImagePath: '',
    heading: '',
    subheading: '',
  },
};

// TODO: crbug.com/440400392- Investigate extracting instances of referencing
// read aloud model directly to an observer.

// Handles the business logic for the visual content of the Reading mode panel.
// This class also is responsible for updating read aloud model when the
// visual content changes.
export class ContentController {
  private nodeStore_: NodeStore = NodeStore.getInstance();
  private speechController_: SpeechController = SpeechController.getInstance();
  private logger_: ReadAnythingLogger = ReadAnythingLogger.getInstance();

  private readonly listeners_: ContentListener[] = [];
  private currentState_: ContentState = CONTENT_STATES[ContentType.NO_CONTENT];
  private previousRootId_?: number;

  // The Trusted Types policy is registered globally on the window object.
  // The browser only allows a policy name to be registered once per page load.
  // Making this static ensures that multiple instances of ContentController
  // (which occur frequently during WebUI test runs) share the same policy
  // reference and do not trigger a "Policy already exists" TypeError.
  private static trustedUpdatePolicy: TrustedTypePolicy|undefined;

  getState(): ContentState {
    return this.currentState_;
  }

  setState(type: ContentType) {
    if (type === this.currentState_.type) {
      return;
    }
    this.currentState_ = CONTENT_STATES[type];
    this.listeners_.forEach(l => l.onContentStateChange());
  }

  hasContent(): boolean {
    return this.currentState_.type === ContentType.HAS_CONTENT;
  }

  isEmpty(): boolean {
    return this.currentState_.type === ContentType.NO_CONTENT ||
        this.currentState_.type === ContentType.NO_SELECTABLE_CONTENT;
  }

  setEmpty() {
    const noContentType = this.getNoContentType_();
    if (this.isEmpty() && this.currentState_.type === noContentType) {
      return;
    }
    // Log the empty state only after a short delay. Sometimes the empty state
    // is only shown very briefly before the content is distilled, so we don't
    // need to count those instances as a failure to distill.
    setTimeout(() => {
      if (this.isEmpty()) {
        this.logger_.logEmptyState();
      }
    }, LOG_EMPTY_DELAY_MS);
    this.setState(noContentType);
  }

  private getNoContentType_() {
    return chrome.readingMode.isGoogleDocs ? ContentType.NO_SELECTABLE_CONTENT :
                                             ContentType.NO_CONTENT;
  }

  addListener(listener: ContentListener) {
    this.listeners_.push(listener);
  }

  onNodeWillBeDeleted(nodeId: number) {
    const deletedNode = this.nodeStore_.getDomNode(nodeId) as ChildNode;

    // When a node is deleted, the read aloud model can get out of sync with the
    // DOM. To be safe, delete the node from the model.
    if (deletedNode && chrome.readingMode.isTsTextSegmentationEnabled) {
      getReadAloudModel().onNodeWillBeDeleted?.(deletedNode);
    }

    if (deletedNode) {
      this.nodeStore_.removeDomNode(deletedNode);
      deletedNode.remove();
      this.listeners_.forEach(l => l.onContentChange());
    }
    const root = this.nodeStore_.getDomNode(chrome.readingMode.rootId);
    if (this.hasContent() && !root?.textContent) {
      this.setState(this.getNoContentType_());
      chrome.readingMode.onNoTextContent();
    }
  }

  updateContent(shadowRoot?: ShadowRoot): Node|null {
    // This shouldn't happen. If it does, there is likely a bug, so log it so
    // we can monitor it.
    if (this.speechController_.isSpeechActive()) {
      console.error(
          'updateContent called while speech is active. ',
          'There may be a bug.');
      this.logger_.logSpeechStopSource(
          chrome.readingMode.unexpectedUpdateContentStopSource);
    }

    this.speechController_.saveReadAloudState();
    this.speechController_.resetForNewContent();
    this.nodeStore_.clearDomNodes();

    if (isDistilledByReadability()) {
      return this.updateContentForReadability();
    }
    return this.updateContentForScreen2x(shadowRoot);
  }

  updateContentForReadability(): Node|null {
    if (isDistilledByReadability()) {
      // Readability Path: Build DOM from the HTML string.
      const title = chrome.readingMode.htmlTitle;
      const contentHtml = chrome.readingMode.htmlContent;

      if (!contentHtml) {
        this.setEmpty();
        return null;
      }

      const contentFragment = document.createDocumentFragment();

      if (title) {
        const titleElement = document.createElement('h1');
        titleElement.textContent = title;
        contentFragment.appendChild(titleElement);
      }

      const contentContainer = document.createElement('div');
      contentContainer.innerHTML = this.getTrustedHtml(contentHtml);

      // Replace tags that shouldn't be interactive or have special behavior
      // in reading mode. This is similar to what happens in `buildSubtree_`
      // for Screen2x.
      for (const [tag, replacement] of READABILITY_TAG_TO_RM_TAG) {
        const elements = contentContainer.querySelectorAll(tag);
        for (const element of elements) {
          const replacementEl = document.createElement(replacement);
          while (element.firstChild) {
            replacementEl.appendChild(element.firstChild);
          }
          for (const attr of element.attributes) {
            replacementEl.setAttribute(attr.name, attr.value);
          }
          element.replaceWith(replacementEl);
        }
      }

      // Set before updateImages to avoid early return.
      this.setState(ContentType.HAS_CONTENT);

      // Process images from distillation.
      this.updateImages(contentContainer);
      contentFragment.appendChild(contentContainer);

      // Ensure link visibility is updated with user preferences.
      this.updateLinksForReadability(contentContainer);

      // TODO(crbug.com/40910704): Remove ReadabilityImageClassifier once we
      // share code with mobile's Reading Mode.
      ReadabilityImageClassifier.processImagesIn(contentContainer);
      this.updateReadAloudState(contentFragment);
      this.listeners_.forEach(l => l.onContentChange());
      return contentFragment;
    }
    return null;
  }

  updateContentForScreen2x(shadowRoot?: ShadowRoot): Node|null {
    // Construct a dom subtree starting with the display root. The display
    // root may be invalid if there are no content nodes and no selection.
    // This does not use Lit's templating abstraction, which would create a
    // shadow node element representing each AXNode, because experimentation
    // (with Polymer) found the shadow node creation to be ~8-10x slower than
    // constructing and appending nodes directly to the container element.
    const rootId = chrome.readingMode.rootId;
    if (!rootId) {
      return null;
    }

    const node = this.buildSubtree_(rootId);
    // If there is no text or images in the tree, do not proceed. The empty
    // state container will show instead.
    if (!node.textContent && !this.nodeStore_.hasImagesToFetch()) {
      // Sometimes the controller thinks there will be content and redraws
      // without showing the empty page, but we end up not actually having any
      // content and also not showing the empty page sometimes. In this case,
      // send that info back to the controller.
      if (this.hasContent()) {
        this.setEmpty();
        chrome.readingMode.onNoTextContent();
      } else if (!this.isEmpty()) {
        // This is possible when the AXTree returns bad selection data and
        // reading mode believes it has selected content to distll but
        // nothing valid is selected. This can cause the loading screen
        // to never switch to the empty state.
        this.setEmpty();
      }
      return null;
    }

    if (this.previousRootId_ !== rootId) {
      this.previousRootId_ = rootId;
      this.logger_.logNewPage(/*speechPlayed=*/ false);
    }

    // Always load images even if they are disabled to ensure a fast response
    // when toggling.
    this.loadImages();
    this.setState(ContentType.HAS_CONTENT);
    this.updateImages(shadowRoot);
    this.listeners_.forEach(l => l.onContentChange());

    this.updateReadAloudState(node);
    return node;
  }

  updateReadAloudState(rootNode: Node): void {
    // If the previous reading position still exists and we haven't reached the
    // end of speech, keep that spot.
    const setPreviousReadingPosition =
        this.speechController_.setPreviousReadingPositionIfExists();
    requestAnimationFrame(() => {
      // Count this as a new page as long as there's no reading position to keep
      // from before.
      if (!setPreviousReadingPosition) {
        this.listeners_.forEach(l => l.onNewPageDrawn());
      }
      this.nodeStore_.estimateWordsSeenWithDelay();
      // Initialize the speech tree with the new content.
      if (chrome.readingMode.isTsTextSegmentationEnabled) {
        const contextNode = ReadAloudNode.create(rootNode);
        if (contextNode) {
          // Don't initialize until after drawing otherwise, the DOM nodes might
          // not yet exist in the tree.
          getReadAloudModel().init(contextNode);
        }
      }
    });
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

    if (SCREEN2X_TAG_TO_RM_TAG.has(htmlTag)) {
      htmlTag = SCREEN2X_TAG_TO_RM_TAG.get(htmlTag)!;
    }

    const url = chrome.readingMode.getUrl(nodeId);
    if (!this.shouldShowLinks_() && htmlTag === LINKS_ON_TAG) {
      htmlTag = LINKS_OFF_TAG;
      dataAttributes.set(LINK_DATA_ATTR, url ?? '');
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

    if (element.nodeName === 'FIGURE') {
      element.style.display = chrome.readingMode.imagesEnabled ? '' : 'none';
    }

    if (url && element.nodeName === 'A') {
      this.setLinkAttributes_(element, url, nodeId);
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

  private setLinkAttributes_(
      element: HTMLElement, url: string, nodeId?: number) {
    element.setAttribute('href', url);
    element.onclick = (event: MouseEvent) => {
      event.preventDefault();
      if (nodeId) {
        chrome.readingMode.onLinkClicked(nodeId);
      }
    };
  }

  private createTextNode_(nodeId: number): Node {
    const textContent = chrome.readingMode.getTextContent(nodeId);
    const textNode = document.createTextNode(textContent);
    this.nodeStore_.setDomNode(textNode, nodeId);
    const isOverline = chrome.readingMode.isOverline(nodeId);
    const shouldBold = chrome.readingMode.shouldBold(nodeId);

    // When creating text nodes, save the first text node id. We need this
    // node id to call InitAXPosition in playSpeech. If it's not saved here,
    // we have to retrieve it through a DOM search such as createTreeWalker,
    // which can be computationally expensive.
    // This needs to be done after the text node is created and added to the
    // node store.
    if (!chrome.readingMode.isTsTextSegmentationEnabled) {
      this.speechController_.initializeSpeechTree(textNode);
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

  updateLinks(shadowRoot?: ShadowRoot) {
    if (!shadowRoot || !this.hasContent()) {
      return;
    }

    const showLinks = this.shouldShowLinks_();
    const selector = showLinks ? LINKS_OFF_SELECTOR : LINKS_ON_TAG;
    const elements = shadowRoot.querySelectorAll<HTMLElement>(selector);
    for (const elem of elements) {
      this.transformLinkContainer_(elem, showLinks);
    }
  }

  // TODO: crbug.com/458961470- This should be merged with updateLinks. This
  // is being kept as a separate method temporarily in order to make things
  // slightly safer for cherrypicking.
  updateLinksForReadability(root?: ParentNode) {
    if (!root || !this.hasContent()) {
      return;
    }

    const showLinks = this.shouldShowLinks_();
    const selector = showLinks ? LINKS_OFF_SELECTOR : LINKS_ON_TAG;
    const elements = root.querySelectorAll<HTMLElement>(selector);
    for (const elem of elements) {
      this.transformLinkContainer_(elem, showLinks);
    }

    // Ensure the link attributes are set initially when reading mode is
    // first opened.
    if (isDistilledByReadability()) {
      const links = root.querySelectorAll('a');
      for (const link of links) {
        const nodeId = this.nodeStore_.getAxId(link);
        this.setLinkAttributes_(link, link.href, nodeId);
      }
    }
  }

  private transformLinkContainer_(
      elemToReplace: HTMLElement, showLinks: boolean) {
    const nodeId = this.nodeStore_.getAxId(elemToReplace);
    // If using Readability distillation, we don't expect there to be an
    // AX id for the node, so don't assert that it exists.
    if (!isDistilledByReadability()) {
      assert(nodeId !== undefined, 'link node id is undefined');
    }
    const newTag = showLinks ? LINKS_ON_TAG : LINKS_OFF_TAG;
    const newElem = document.createElement(newTag);
    // Move children to preserve inner highlighting or other formatting.
    while (elemToReplace.firstChild) {
      // appendChild moves the child to the new element, so the next call to
      // elemToReplace.firstChild will get the next child.
      newElem.appendChild(elemToReplace.firstChild);
    }

    // Copy all attributes from the old element to the new one.
    for (const attrName of elemToReplace.getAttributeNames()) {
      // Skip the attributes we are manually changing.
      if (attrName === 'href' || attrName === DATA_PREFIX + LINK_DATA_ATTR) {
        continue;
      }
      const attrValue = elemToReplace.getAttribute(attrName)!;
      newElem.setAttribute(attrName, attrValue);
    }

    // Set the url information on the new element.
    if (showLinks) {
      const url = elemToReplace.dataset[LINK_DATA_ATTR] ?? '';
      this.setLinkAttributes_(newElem, url, nodeId);
    } else {
      const url = elemToReplace.getAttribute('href') ?? '';
      newElem.dataset[LINK_DATA_ATTR] = url;
    }

    // Remove the highlighting formatting when showing links, and add it back
    // when hiding links if they were highlighted.
    const originalClass =
        showLinks ? previousReadHighlightClass : HIGHLIGHTED_LINK_CLASS;
    const newClass =
        showLinks ? HIGHLIGHTED_LINK_CLASS : previousReadHighlightClass;
    const highlightedNodes =
        Array.from(newElem.querySelectorAll<HTMLElement>('.' + originalClass));
    if (newElem.classList.contains(originalClass)) {
      highlightedNodes.push(newElem);
    }
    highlightedNodes.forEach(node => {
      node.classList.replace(originalClass, newClass);
    });

    this.nodeStore_.replaceDomNode(elemToReplace, newElem);
  }

  // TODO(crbug.com/40910704): Potentially hide links during distillation.
  private shouldShowLinks_(): boolean {
    // If Readability is enabled and the ReadabilityWithLinks flag is disabled,
    // don't show links.
    if (chrome.readingMode.isReadabilityEnabled &&
        !chrome.readingMode.isReadabilityWithLinksEnabled) {
      return false;
    }

    // Links should only show when Read Aloud is paused.
    return chrome.readingMode.linksEnabled &&
        !this.speechController_.isSpeechActive();
  }

  onSelectionChange(shadowRoot?: ShadowRoot) {
    if (!shadowRoot) {
      return;
    }
    const highlightedNodes =
        shadowRoot.querySelectorAll<HTMLElement>('.' + HIGHLIGHTED_LINK_CLASS);
    highlightedNodes.forEach(
        node => node.classList.remove(HIGHLIGHTED_LINK_CLASS));
  }

  loadImages() {
    if (!chrome.readingMode.imagesFeatureEnabled) {
      return;
    }

    this.nodeStore_.fetchImages();
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
      const imgData = new ImageData(
          data.data as Uint8ClampedArray<ArrayBuffer>, data.width);
      const bitmap = await createImageBitmap(imgData, {
        colorSpaceConversion: 'none',
        premultiplyAlpha: 'premultiply',
      });
      context.drawImage(bitmap, 0, 0);
      this.listeners_.forEach(l => l.onContentChange());
    }
  }

  updateImages(root?: ParentNode) {
    if (!root || !this.hasContent()) {
      return;
    }

    const imagesUpdated = isDistilledByReadability() ?
        this.updateImagesForReadability_(root) :
        this.updateImagesForAxTree_(root);
    if (imagesUpdated) {
      this.listeners_.forEach(l => l.onContentChange());
    }
  }

  updateAnchorsForReadability(root: ParentNode) {
    if (!chrome.readingMode.isReadabilityEnabled ||
        !chrome.readingMode.isReadabilityWithLinksEnabled ||
        !isDistilledByReadability()) {
      return;
    }

    if (!root || !this.hasContent()) {
      return;
    }

    const anchors = Array.from(root.querySelectorAll<HTMLAnchorElement>('a'));
    const originalAnchors: Record<string, AxTreeAnchorMetadata[]> =
        chrome.readingMode.axTreeAnchors;
    for (const anchor of anchors) {
      const url = anchor.href;
      if (!url) {
        this.transformLinkContainer_(anchor, false);
        continue;
      }

      const options = originalAnchors[url];
      if (!options) {
        this.transformLinkContainer_(anchor, false);
        continue;
      }

      let matchIndex = null;
      if (options.length === 1) {
        // Use the first item in the list if there is a single anchor for this
        // URL.
        matchIndex = 0;
      } else {
        // Otherwise, try to find an anchor that matches.
        matchIndex = this.findStrictMatch_(anchor, options);
      }

      if (matchIndex === null || matchIndex >= options.length) {
        // Convert the anchor to text if no match is found.
        this.transformLinkContainer_(anchor, false);
        continue;
      }

      const match = options[matchIndex];
      if (!match) {
        continue;
      }

      const lastIndex = options.length - 1;
      const lastElement = options[lastIndex];
      if (lastElement !== undefined) {
        // If there is a match, remove it from the list of options so it
        // cannot be reused by another anchor.
        options[matchIndex] = lastElement;
        options.pop();
      }

      const nodeID = match.axId;
      this.nodeStore_.setDomNode(anchor, nodeID);
      this.setLinkAttributes_(anchor, url, nodeID);
    }
  }

  private findStrictMatch_(
      domNode: HTMLAnchorElement, candidates: AxTreeAnchorMetadata[]): number
      |null {
    let bestCandidateIndex: number|null = null;
    let highestScore = -1;
    let tieDetected = false;

    for (let i = 0; i < candidates.length; i++) {
      const metaCandidate = candidates[i];
      if (!metaCandidate) {
        continue;
      }

      const score = this.calculateMatchScore_(domNode, metaCandidate);
      if (score > highestScore) {
        highestScore = score;
        bestCandidateIndex = i;
        tieDetected = false;
      } else if (score === highestScore && score > 0) {
        tieDetected = true;
      }
    }

    if (tieDetected) {
      return null;
    }

    return bestCandidateIndex;
  }


  // Calculates a heuristic match score to bind a distilled DOM node to the
  // correct AXNodeID.
  // Note: The score values below are heuristics based on the relative
  // importance of different signals. They can be tuned or changed in the future
  // if needed.
  //
  // High-level scoring hierarchy:
  // 1. HTML ID: Highest confidence (guaranteed match).
  // 2. Visible Text: Strongest indicator of user intent.
  // 3. Surrounding Context: Heavy tie-breaker to disambiguate identical generic
  //    links (e.g., "Read More").
  // 4. Attributes (Title/Target): Micro tie-breakers for edge cases.
  private calculateMatchScore_(
      domNode: HTMLAnchorElement, axLink: AxTreeAnchorMetadata): number {
    if (!domNode || !axLink) {
      return 0;
    }

    if (axLink.htmlId && domNode.id && axLink.htmlId === domNode.id) {
      // 10,000: Treated as an exact match.
      return 10000;
    }

    let score = 0;
    const domText = (domNode.textContent || '').trim();
    const metaText = (axLink.name || '').trim();
    if (domText && metaText) {
      if (domText === metaText) {
        // +60: Exact text match. Weighted high enough so a perfect text match
        // without context beats a partial match with perfect context
        // (15 + 50 = 65).
        score += 60;
      } else if (domText.includes(metaText) || metaText.includes(domText)) {
        // +15: Partial text match. Needs strong context to win.
        score += 15;
      }
    }

    // Context readability: Check adjacent text nodes to disambiguate identical
    // links.
    const domPrev = (domNode.previousSibling?.textContent || '').trim();
    const metaPrev = (axLink.textBefore || '').trim();
    if (metaPrev && domPrev) {
      if (domPrev.endsWith(metaPrev) || metaPrev.endsWith(domPrev)) {
        // +25: Previous sibling match. Heavy tie-breaker.
        score += 25;
      }
    }
    const domNext = (domNode.nextSibling?.textContent || '').trim();
    const metaNext = (axLink.textAfter || '').trim();
    if (metaNext && domNext) {
      if (domNext.startsWith(metaNext) || metaNext.startsWith(domNext)) {
        // +25: Next sibling match. Heavy tie-breaker.
        score += 25;
      }
    }

    // Micro tie-breakers: Low weight to never override primary text/context
    // signals.
    if (axLink.title && domNode.title && axLink.title === domNode.title) {
      score += 7;
    }
    if (axLink.target && domNode.target &&
        axLink.target.toLowerCase() === domNode.target.toLowerCase()) {
      score += 3;
    }

    return score;
  }

  private updateImagesForAxTree_(shadowRoot: ParentNode): boolean {
    if (!chrome.readingMode.imagesFeatureEnabled) {
      return false;
    }

    const imagesEnabled = chrome.readingMode.imagesEnabled;
    if (imagesEnabled) {
      this.nodeStore_.clearHiddenImageNodes();
    }
    // There is some strange issue where the HTML css application does not work
    // on canvases.
    const canvases = shadowRoot.querySelectorAll<HTMLElement>('canvas, figure');
    for (const canvas of canvases) {
      canvas.style.display = imagesEnabled ? '' : 'none';
      this.markTextNodesHiddenIfImagesHidden_(canvas);
    }
    return canvases.length > 0;
  }

  private updateImagesForReadability_(container: ParentNode): boolean {
    if (!isDistilledByReadability()) {
      return false;
    }

    // If chrome.readingMode.imagesFeatureEnabled is disabled, hide images also.
    const imagesEnabled = chrome.readingMode.imagesEnabled &&
        chrome.readingMode.imagesFeatureEnabled;
    // We look for 'img' tags (standard HTML) and 'figure' tags like screen2x.
    const images = container.querySelectorAll<HTMLElement>('img, figure');

    for (const element of images) {
      element.style.display = imagesEnabled ? '' : 'none';
    }

    return images.length > 0;
  }

  private async markTextNodesHiddenIfImagesHidden_(node: Node) {
    if (chrome.readingMode.imagesEnabled) {
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

  private getTrustedHtml(html: string): TrustedHTML {
    if (!ContentController.trustedUpdatePolicy || !isDistilledByReadability()) {
      return window.trustedTypes!.emptyHTML;
    }
    return ContentController.trustedUpdatePolicy.createHTML(html);
  }

  configureTrustedTypes(): void {
    if (!window.trustedTypes || ContentController.trustedUpdatePolicy) {
      return;
    }
    ContentController.trustedUpdatePolicy =
        window.trustedTypes.createPolicy('reader-mode-policy', {
          createHTML: (s: string) => s,
          createScript: () => '',
          createScriptURL: () => '',
        });
  }

  static getInstance(): ContentController {
    return instance || (instance = new ContentController());
  }

  static setInstance(obj: ContentController) {
    instance = obj;
  }
}

let instance: ContentController|null = null;
