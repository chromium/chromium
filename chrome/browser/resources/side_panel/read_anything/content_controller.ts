// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';

import {NodeStore} from './node_store.js';

// Handles the business logic for the visual content of the Reading mode panel.
export class ContentController {
  private nodeStore_: NodeStore = NodeStore.getInstance();

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
          console.error('is text node');
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
