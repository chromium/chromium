// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import {CenterRotatedBox_CoordinateType} from './geometry.mojom-webui.js';
import type {Text, Word} from './text.mojom-webui.js';
import {getTemplate} from './text_layer.html.js';

// Takes the value between 0-1 and returns a string in the from '__%';
function toPercent(value: number): string {
  return `${value * 100}%`;
}

// Returns true if the word has a valid bounding box and is renderable by the
// TextLayer.
function isWordRenderable(word: Word): boolean {
  // For a word to be renderable, it must have a bounding box with normalized
  // coordinates.
  // TODO(b/330183480): Add rendering for IMAGE CoordinateType
  const wordBoundingBox = word.geometry?.boundingBox;
  if (!wordBoundingBox) {
    return false;
  }

  return wordBoundingBox.coordinateType ===
      CenterRotatedBox_CoordinateType.kNormalized;
}

/*
 * Element responsible for highlighting and selection text.
 */
export class TextLayerElement extends PolymerElement {
  static get is() {
    return 'lens-text-layer';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      renderedWords: {
        type: Array,
        value: () => [],
      },
    };
  }

  // The words rendered in this layer.
  private renderedWords: Word[] = [];

  override ready() {
    super.ready();

    // Set up listener to receive text from C++
    BrowserProxyImpl.getInstance().callbackRouter.textReceived.addListener(
        this.onTextReceived.bind(this));
  }

  private onTextReceived(text: Text) {
    const receivedWords = [];
    // Flatten Text structure to an array of words.
    for (const paragraph of text.textLayout.paragraphs) {
      for (const line of paragraph.lines) {
        for (const word of line.words) {
          // Filter out words with invalid bounding boxes.
          if (isWordRenderable(word)) {
            receivedWords.push(word);
          }
        }
      }
    }
    // Need to set this.renderedWords to a new array instead of
    // this.renderedWords.push() to ensure the dom-repeat updates.
    this.renderedWords = receivedWords;
  }

  /** @return The CSS styles string for the given word. */
  private getWordStyle_(word: Word): string {
    // Words without bounding boxes are filtered out, so guaranteed that
    // geometry is not null.
    const wordBoundingBox = word.geometry!.boundingBox;

    // TODO(b/330183480): Currently, we are assuming that word
    // coordinates are normalized. We should still implement
    // rendering in case this assumption is ever violated.
    if (wordBoundingBox.coordinateType !==
        CenterRotatedBox_CoordinateType.kNormalized) {
      return '';
    }

    // Put into an array instead of a long string to keep this code readable.
    const styles: string[] = [
      `width: ${toPercent(wordBoundingBox.box.width)}`,
      `height: ${toPercent(wordBoundingBox.box.height)}`,
      `top: ${
          toPercent(wordBoundingBox.box.y - (wordBoundingBox.box.height / 2))}`,
      `left: ${
          toPercent(wordBoundingBox.box.x - (wordBoundingBox.box.width / 2))}`,
      `transform: rotate(${wordBoundingBox.rotation}rad)`,
    ];
    return styles.join(';');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'lens-text-layer': TextLayerElement;
  }
}

customElements.define(TextLayerElement.is, TextLayerElement);
