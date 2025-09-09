// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';

import {NodeStore} from './node_store.js';
import {previousReadHighlightClass} from './read_aloud/movement.js';
import {SpeechController} from './read_aloud/speech_controller.js';

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
const TAG_TO_RM_TAG: Map<string, string> = new Map([
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
]);

// Handles the business logic for the visual content of the Reading mode panel.
export class ContentController {
  private nodeStore_: NodeStore = NodeStore.getInstance();
  private speechController_: SpeechController = SpeechController.getInstance();

  buildSubtree(nodeId: number): Node {
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

    if (TAG_TO_RM_TAG.has(htmlTag)) {
      htmlTag = TAG_TO_RM_TAG.get(htmlTag)!;
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
      const childNode = this.buildSubtree(childNodeId);
      node.appendChild(childNode);
    }
  }

  private setLinkAttributes_(
      element: HTMLElement, url: string, nodeId: number) {
    element.setAttribute('href', url);
    element.onclick = (event: MouseEvent) => {
      event.preventDefault();
      chrome.readingMode.onLinkClicked(nodeId);
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
    if (chrome.readingMode.isReadAloudEnabled &&
        !chrome.readingMode.isTsTextSegmentationEnabled) {
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

  updateLinks(hasContent: boolean, shadowRoot?: ShadowRoot) {
    if (!shadowRoot || !hasContent) {
      return;
    }

    const showLinks = this.shouldShowLinks_();
    const selector = showLinks ? LINKS_OFF_SELECTOR : LINKS_ON_TAG;
    const elements = shadowRoot.querySelectorAll<HTMLElement>(selector);
    for (const elem of elements) {
      this.transformLinkContainer_(elem, showLinks);
    }
  }

  private transformLinkContainer_(
      elemToReplace: HTMLElement, showLinks: boolean) {
    const nodeId = this.nodeStore_.getAxId(elemToReplace);
    assert(nodeId !== undefined, 'link node id is undefined');
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
      const imgData = new ImageData(data.data, data.width);
      const bitmap = await createImageBitmap(imgData, {
        colorSpaceConversion: 'none',
        premultiplyAlpha: 'premultiply',
      });
      context.drawImage(bitmap, 0, 0);
    }
  }

  updateImages(hasContent: boolean, shadowRoot?: ShadowRoot) {
    if (!shadowRoot || !chrome.readingMode.imagesFeatureEnabled ||
        !hasContent) {
      return;
    }

    const imagesEnabled = chrome.readingMode.imagesEnabled;
    if (imagesEnabled) {
      this.nodeStore_.clearHiddenImageNodes();
    }
    // There is some strange issue where the HTML css application does not work
    // on canvases.
    for (const canvas of shadowRoot.querySelectorAll('canvas')) {
      canvas.style.display = imagesEnabled ? '' : 'none';
      this.markTextNodesHiddenIfImagesHidden_(canvas);
    }
    for (const canvas of shadowRoot.querySelectorAll('figure')) {
      canvas.style.display = imagesEnabled ? '' : 'none';
      this.markTextNodesHiddenIfImagesHidden_(canvas);
    }
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

  static getInstance(): ContentController {
    return instance || (instance = new ContentController());
  }

  static setInstance(obj: ContentController) {
    instance = obj;
  }
}

let instance: ContentController|null = null;
