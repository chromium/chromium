// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {PointF} from '//resources/mojo/ui/gfx/geometry/mojom/geometry.mojom-webui.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {DomRepeat} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import {CenterRotatedBox_CoordinateType} from './geometry.mojom-webui.js';
import {bestHit} from './hit.js';
import type {CursorData, TextContextMenuData} from './selection_overlay.js';
import {CursorType} from './selection_utils.js';
import type {GestureEvent} from './selection_utils.js';
import type {Line, Paragraph, Text, Word} from './text.mojom-webui.js';
import {WritingDirection} from './text.mojom-webui.js';
import {getTemplate} from './text_layer.html.js';
import {toPercent} from './values_converter.js';

// Rotates the target coordinates to be in relation to the line rotation.
function rotateCoordinateAroundOrigin(
    pointToRotate: PointF, angle: number): PointF {
  const newX =
      pointToRotate.x * Math.cos(-angle) - pointToRotate.y * Math.sin(-angle);
  const newY =
      pointToRotate.y * Math.cos(-angle) + pointToRotate.x * Math.sin(-angle);
  return {x: newX, y: newY};
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

// Return the text separator if there is one, else returns a space.
function getTextSeparator(word: Word): string {
  return word.textSeparator ? word.textSeparator : ' ';
}

export interface TextLayerElement {
  $: {
    wordsContainer: DomRepeat,
  };
}

interface HighlightedLine {
  height: number;
  left: number;
  top: number;
  width: number;
  rotation: number;
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
      highlightedLines: {
        type: Array,
        computed: 'getHighlightedLines(selectionStartIndex, selectionEndIndex)',
      },
      selectionStartIndex: {
        type: Number,
        value: -1,
      },
      selectionEndIndex: {
        type: Number,
        value: -1,
      },
      isSelectingText: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },
      debugMode: {
        type: Boolean,
        value: loadTimeData.getBoolean('enableDebuggingMode'),
        reflectToAttribute: true,
      },
    };
  }

  // The words rendered in this layer.
  private renderedWords: Word[];
  // The currently selected lines.
  private highlightedLines: HighlightedLine[];
  // The index of the word in renderedWords at the start of the current
  // selection. -1 if no current selection.
  private selectionStartIndex: number;
  // The index of the word in renderedWords at the end of the current selection.
  // -1 if no current selection.
  private selectionEndIndex: number;
  // Whether the user is currently selecting text.
  private isSelectingText: boolean;

  // An array that corresponds 1:1 to renderedWords, where lineNumbers[i] is the
  // line number for renderedWords[i]. In addition, the index at lineNumbers[i]
  // corresponds to the Line in lines[i] that the word belongs in.
  private lineNumbers: number[];
  // An array that corresponds 1:1 to renderedWords, where paragraphNumbers[i]
  // is the paragraph number for renderedWords[i]. In addition, the index at
  // paragraphNumbers[i] corresponds to the Paragraph in paragraphs[i] that the
  // word belongs in.
  private paragraphNumbers: number[];
  // The lines received from OnTextReceived.
  private lines: Line[];
  // The paragraphs received from OnTextReceived.
  private paragraphs: Paragraph[];
  private listenerIds: number[];

  override connectedCallback() {
    super.connectedCallback();

    // Set up listener to listen to events from C++.
    this.listenerIds = [
      BrowserProxyImpl.getInstance().callbackRouter.textReceived.addListener(
          this.onTextReceived.bind(this)),
      BrowserProxyImpl.getInstance()
          .callbackRouter.clearAllSelections.addListener(
              this.unselectWords.bind(this)),
      BrowserProxyImpl.getInstance()
          .callbackRouter.setTextSelection.addListener(
              this.selectWords.bind(this)),
    ];
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    this.listenerIds.forEach(
        id => assert(
            BrowserProxyImpl.getInstance().callbackRouter.removeListener(id)));
    this.listenerIds = [];
  }

  private handlePointerEnter() {
    this.dispatchEvent(new CustomEvent<CursorData>(
        'set-cursor',
        {bubbles: true, composed: true, detail: {cursor: CursorType.TEXT}}));
  }

  private handlePointerLeave() {
    this.dispatchEvent(new CustomEvent<CursorData>(
        'set-cursor',
        {bubbles: true, composed: true, detail: {cursor: CursorType.DEFAULT}}));
  }

  handleDownGesture(event: GestureEvent): boolean {
    this.unselectWords();

    const wordIndex = this.wordIndexFromPoint(event.clientX, event.clientY);
    // Ignore if the click is not on a word.
    if (wordIndex === null) {
      return false;
    }

    this.selectionStartIndex = wordIndex;
    this.selectionEndIndex = wordIndex;
    this.isSelectingText = true;
    return true;
  }

  handleDragGesture(event: GestureEvent) {
    const imageBounds = this.getBoundingClientRect();
    const normalizedX = (event.clientX - imageBounds.left) / imageBounds.width;
    const normalizedY = (event.clientY - imageBounds.top) / imageBounds.height;

    const hit = bestHit(this.renderedWords, {x: normalizedX, y: normalizedY});

    if (!hit) {
      return;
    }

    this.selectionEndIndex = this.renderedWords.indexOf(hit);
  }

  handleUpGesture() {
    this.isSelectingText = false;
    const highlightedText = this.getHighlightedText();
    const lines = this.getHighlightedLines();
    const containingRect = this.getContainingRect(lines);
    this.dispatchEvent(
        new CustomEvent<TextContextMenuData>('show-text-context-menu', {
          bubbles: true,
          composed: true,
          detail: {
            text: highlightedText,
            left: containingRect.left,
            right: containingRect.right,
            top: containingRect.top,
            bottom: containingRect.bottom,
            selectionStartIndex: this.selectionStartIndex,
            selectionEndIndex: this.selectionEndIndex,
          },
        }));

    // On selection complete, send the selected text to C++.
    BrowserProxyImpl.getInstance().handler.issueTextSelectionRequest(
        highlightedText, this.selectionStartIndex, this.selectionEndIndex);
  }

  cancelGesture() {
    this.unselectWords();
  }

  private unselectWords() {
    this.selectionStartIndex = -1;
    this.selectionEndIndex = -1;
    this.dispatchEvent(new CustomEvent(
        'hide-text-context-menu', {bubbles: true, composed: true}));
  }

  private selectWords(selectionStartIndex: number, selectionEndIndex: number) {
    this.selectionStartIndex = selectionStartIndex;
    this.selectionEndIndex = selectionEndIndex;
  }

  private onTextReceived(text: Text) {
    // Reset all old text.
    const receivedWords = [];
    this.lineNumbers = [];
    this.paragraphNumbers = [];
    this.lines = [];
    this.paragraphs = [];
    let lineNumber = 0;
    let paragraphNumber = 0;

    // Flatten Text structure to a list of arrays for easier rendering and
    // referencing.
    for (const paragraph of text.textLayout.paragraphs) {
      for (const line of paragraph.lines) {
        for (const word of line.words) {
          // Filter out words with invalid bounding boxes.
          if (isWordRenderable(word)) {
            receivedWords.push(word);
            this.lineNumbers.push(lineNumber);
            this.paragraphNumbers.push(paragraphNumber);
          }
        }
        this.lines.push(line);
        lineNumber++;
      }
      this.paragraphs.push(paragraph);
      paragraphNumber++;
    }
    // Need to set this.renderedWords to a new array instead of
    // this.renderedWords.push() to ensure the dom-repeat updates.
    this.renderedWords = receivedWords;
    assert(this.lineNumbers.length === this.renderedWords.length);
    assert(this.paragraphNumbers.length === this.renderedWords.length);
  }

  // Returns the rectangle circumscribing the given lines.
  private getContainingRect(lines: HighlightedLine[]) {
    const left = Math.min(...lines.map((line) => line.left));
    const right = Math.max(...lines.map((line) => line.left + line.width));
    const top = Math.min(...lines.map((line) => line.top));
    const bottom = Math.max(...lines.map((line) => line.top + line.height));
    return {left, right, top, bottom};
  }

  // Used by the HTML template to get the array of highlighted lines to render
  // whenever the selection indices change.
  private getHighlightedLines(): HighlightedLine[] {
    const newHighlightedLines: HighlightedLine[] = [];

    // Return early if there isn't a valid selection.
    if (this.selectionStartIndex === -1 || this.selectionEndIndex === -1) {
      return newHighlightedLines;
    }

    const startIndex =
        Math.min(this.selectionStartIndex, this.selectionEndIndex);
    const endIndex = Math.max(this.selectionStartIndex, this.selectionEndIndex);

    let currentLineIndex = this.lineNumbers[startIndex];
    let startWord: Word = this.renderedWords[startIndex];
    let endWord: Word = this.renderedWords[startIndex];

    // Get max dimensions per line.
    for (let i = startIndex; i <= endIndex; i++) {
      if (this.lineNumbers[i] !== currentLineIndex) {
        // Add the line
        newHighlightedLines.push(this.calculateHighlightedLine(
            startWord, endWord, this.isTopToBottomWritingDirection(i)));

        // Save new line data.
        startWord = this.renderedWords[i];
        currentLineIndex = this.lineNumbers[i];
      }
      endWord = this.renderedWords[i];
    }
    // Add the last line in the selection
    newHighlightedLines.push(this.calculateHighlightedLine(
        startWord, endWord, this.isTopToBottomWritingDirection(endIndex)));

    return newHighlightedLines;
  }

  // Given two words, returns the bounding box that properly encapsulates this
  // region.
  private calculateHighlightedLine(
      startWord: Word, endWord: Word, isTopToBottom: boolean): HighlightedLine {
    // We only render words with geometry, so these geometry's should be
    // guaranteed to exist.
    assert(startWord.geometry);
    assert(endWord.geometry);

    // Grab the bounding boxes for easier to read code
    const startWordBoundingBox = startWord.geometry.boundingBox;
    const endWordBoundingBox = endWord.geometry.boundingBox;

    // Since the two words in a line can be at an angle, there center points are
    // not necessarily in a straight line. We need to calculate the slope
    // created by the selected boxes to align the boxes vertically so we can
    // generate the containing box.
    const slope = (endWordBoundingBox.box.y - startWordBoundingBox.box.y) /
        (endWordBoundingBox.box.x - startWordBoundingBox.box.x);

    // Calculate the angle needed to rotate to align the items linearly. If
    // slope is undefined because the denominator was zero, we default to no
    // rotation.
    let rotationAngle = slope ? Math.atan(slope) : 0;

    // Top to bottom languages need to rotate by an extra 90 degrees for the
    // logic to work correctly.
    if (isTopToBottom) {
      rotationAngle += 1.5708;
    }

    // Get the new linearly aligned center points.
    const relativeStartCenter = rotateCoordinateAroundOrigin(
        {x: startWordBoundingBox.box.x, y: startWordBoundingBox.box.y},
        rotationAngle);
    const relativeEndCenter = rotateCoordinateAroundOrigin(
        {x: endWordBoundingBox.box.x, y: endWordBoundingBox.box.y},
        rotationAngle);

    // Calculate the dimensions for our containing box using the new center
    // points and the same width and height as before.
    const containingBoxTop = Math.min(
        relativeStartCenter.y - startWordBoundingBox.box.height / 2,
        relativeEndCenter.y - endWordBoundingBox.box.height / 2);
    const containingBoxLeft = Math.min(
        relativeStartCenter.x - startWordBoundingBox.box.width / 2,
        relativeEndCenter.x - endWordBoundingBox.box.width / 2);
    const containingBoxBottom = Math.max(
        relativeStartCenter.y + startWordBoundingBox.box.height / 2,
        relativeEndCenter.y + endWordBoundingBox.box.height / 2);
    const containingBoxRight = Math.max(
        relativeStartCenter.x + startWordBoundingBox.box.width / 2,
        relativeEndCenter.x + endWordBoundingBox.box.width / 2);

    // The generate the center point and undo the rotation so it is back to
    // being relative to the position of the selected line.
    const containingCenter = rotateCoordinateAroundOrigin(
        {
          x: (containingBoxRight + containingBoxLeft) / 2,
          y: (containingBoxTop + containingBoxBottom) / 2,
        },
        -rotationAngle);

    // Since width and height don't change with rotation, simply get the width
    // and height.
    const containingBoxWidth = containingBoxRight - containingBoxLeft;
    const containingBoxHeight = containingBoxBottom - containingBoxTop;

    // Convert to easy to render format.
    return {
      top: containingCenter.y - containingBoxHeight / 2,
      left: containingCenter.x - containingBoxWidth / 2,
      width: containingBoxWidth,
      height: containingBoxHeight,
      rotation:
          (startWordBoundingBox.rotation + endWordBoundingBox.rotation) / 2,
    };
  }

  // Returns whether the word at the given index is a top to bottom written
  // language.
  private isTopToBottomWritingDirection(wordIndex: number): boolean {
    const paragraph = this.paragraphs[this.paragraphNumbers[wordIndex]];
    return paragraph.writingDirection === WritingDirection.kTopToBottom;
  }

  private getHighlightedText(): string {
    // Return early if there isn't a valid selection.
    if (this.selectionStartIndex === -1 || this.selectionEndIndex === -1) {
      return '';
    }

    const startIndex =
        Math.min(this.selectionStartIndex, this.selectionEndIndex);
    const endIndex = Math.max(this.selectionStartIndex, this.selectionEndIndex);

    const selectedWords = this.renderedWords.slice(startIndex, endIndex + 1);
    return selectedWords
        .map((word, index) => {
          return word.plainText +
              (index < selectedWords.length - 1 ? getTextSeparator(word) : '');
        })
        .join('');
  }

  /** @return The CSS styles string for the given word. */
  private getWordStyle(word: Word): string {
    const parentRect = this.getBoundingClientRect();
    const horizontalLineMarginPercent =
        loadTimeData.getInteger('verticalTextMarginPx') / parentRect.width;
    const verticalLineMarginPercent =
        loadTimeData.getInteger('horizontalTextMarginPx') / parentRect.height;

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
      `width: ${
          toPercent(
              wordBoundingBox.box.width + 2 * horizontalLineMarginPercent)}`,
      `height: ${
          toPercent(
              wordBoundingBox.box.height + 2 * verticalLineMarginPercent)}`,
      `top: ${
          toPercent(
              wordBoundingBox.box.y - (wordBoundingBox.box.height / 2) -
              verticalLineMarginPercent)}`,
      `left: ${
          toPercent(
              wordBoundingBox.box.x - (wordBoundingBox.box.width / 2) -
              horizontalLineMarginPercent)}`,
      `transform: rotate(${wordBoundingBox.rotation}rad)`,
    ];
    return styles.join(';');
  }

  /** @return The CSS styles string for the given highlighted line. */
  private getHighlightedLineStyle(line: HighlightedLine): string {
    // Put into an array instead of a long string to keep this code readable.
    const styles: string[] = [
      `width: ${toPercent(line.width)}`,
      `height: ${toPercent(line.height)}`,
      `top: ${toPercent(line.top)}`,
      `left: ${toPercent(line.left)}`,
      `transform: rotate(${line.rotation}rad)`,
    ];
    return styles.join(';');
  }

  /**
   * @return Returns the index in renderedWords of the word at the given point.
   *     Returns null if no word is at the given point.
   */
  private wordIndexFromPoint(x: number, y: number): number|null {
    const topMostElement = this.shadowRoot!.elementFromPoint(x, y);
    if (!topMostElement || !(topMostElement instanceof HTMLElement)) {
      return null;
    }
    return this.$.wordsContainer.indexForElement(topMostElement);
  }

  // Testing method to get the words on the page.
  getWordNodesForTesting() {
    return this.shadowRoot!.querySelectorAll('.word');
  }

  // Testing method to get the highlighted words on the page.
  getHighlightedNodesForTesting() {
    return this.shadowRoot!.querySelectorAll('.highlighted-line');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'lens-text-layer': TextLayerElement;
  }
}

customElements.define(TextLayerElement.is, TextLayerElement);
