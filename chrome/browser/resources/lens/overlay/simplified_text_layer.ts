// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {assert} from '//resources/js/assert.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import type {BrowserProxy} from './browser_proxy.js';
import type {CenterRotatedBox} from './geometry.mojom-webui.js';
import {SemanticEvent} from './lens.mojom-webui.js';
import {recordLensOverlaySemanticEvent} from './metrics_utils.js';
import type {GestureEvent} from './selection_utils.js';
import {getCss} from './simplified_text_layer.css.js';
import {getHtml} from './simplified_text_layer.html.js';
import type {Line, Paragraph, Text, Word} from './text.mojom-webui.js';
import type {TextLayerBase} from './text_layer_base.js';
import {getTextSeparator, isWordRenderable, translateWords} from './text_rendering.js';

/*
 * Element responsible for highlighting and selection text.
 */
export class SimplifiedTextLayerElement extends CrLitElement implements
    TextLayerBase {
  static get is() {
    return 'lens-simplified-text-layer';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  // The content language received from OnTextReceived.
  private contentLanguage: string;
  // The words received in this layer.
  private receivedWords: Word[] = [];
  // An array that corresponds 1:1 to receivedWords, where paragraphNumbers[i]
  // is the paragraph number for receivedWords[i]. In addition, the index at
  // paragraphNumbers[i] corresponds to the Paragraph in paragraphs[i] that the
  // word belongs in.
  private paragraphNumbers: number[] = [];
  // The lines received from OnTextReceived.
  private lines: Line[] = [];
  // The paragraphs received from OnTextReceived.
  private paragraphs: Paragraph[] = [];
  private eventTracker_: EventTracker = new EventTracker();
  private listenerIds: number[] = [];
  // Timeout for onTextReceived. The selected region context menu should not be
  // shown until either the text is received or the timeout elapses.
  private textReceivedTimeout: number =
      loadTimeData.getValue('textReceivedTimeout');
  private textReceivedTimeoutId: number = -1;
  private textReceivedTimeoutElapsedOrCleared: boolean = false;
  private browserProxy: BrowserProxy = BrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();
    this.eventTracker_.add(
        document, 'detect-text-in-region',
        (e: CustomEvent<CenterRotatedBox>) => {
          this.detectTextInRegion(e.detail);
        });

    // Set up listener to listen to events from C++.
    this.listenerIds = [
      this.browserProxy.callbackRouter.textReceived.addListener(
          this.onTextReceived.bind(this)),
    ];
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    // If there was rendered text, log a text gleam render end event.
    if (this.receivedWords.length > 0) {
      recordLensOverlaySemanticEvent(SemanticEvent.kTextGleamsViewEnd);
    }

    this.listenerIds.forEach(id => {
      const removed = this.browserProxy.callbackRouter.removeListener(id);
      assert(removed);
    });
    this.listenerIds = [];
    this.eventTracker_.removeAll();
  }

  handleRightClick(_event: PointerEvent): boolean {
    // Do nothing. Gestures are currently not used in this layer.
    return false;
  }

  handleGestureStart(_event: GestureEvent): boolean {
    // Do nothing. Gestures are currently not used in this layer.
    return false;
  }

  handleGestureDrag(_event: GestureEvent) {
    // Do nothing. Gestures are currently not used in this layer.
  }

  handleGestureEnd() {
    // Do nothing. Gestures are currently not used in this layer.
  }

  cancelGesture() {
    // Do nothing. Gestures are currently not used in this layer.
  }

  onSelectionStart(): void {
    this.fire('hide-selected-region-context-menu');
  }

  onSelectionFinish(): void {
    this.receivedWords = [];
    this.contentLanguage = '';
    this.setTextReceivedTimeout();
  }

  selectAndSendWords(_selectionStartIndex: number, _selectionEndIndex: number) {
    // Do nothing. The simplified text layer does not support selecting words.
  }

  selectAndTranslateWords(startIndex: number, endIndex: number) {
    // This layer does not support selection of text. So just translate the
    // words.
    translateWords(
        this.getRegionText(), this.contentLanguage, startIndex, endIndex,
        this.browserProxy);
  }

  private setTextReceivedTimeout() {
    this.textReceivedTimeoutElapsedOrCleared = false;
    this.textReceivedTimeoutId = setTimeout(() => {
      this.textReceivedTimeoutElapsedOrCleared = true;
      this.textReceivedTimeoutId = -1;
    }, this.textReceivedTimeout);
  }

  private detectTextInRegion(box: CenterRotatedBox) {
    // If we are still waiting for the text, hide the context menu.
    if (!this.textReceivedTimeoutElapsedOrCleared) {
      this.fire('hide-selected-region-context-menu');
      return;
    }

    if (this.receivedWords.length <= 0) {
      this.fire(
          'show-selected-region-context-menu',
          {box, selectionStartIndex: -1, selectionEndIndex: -1});
      return;
    }

    this.fire('show-selected-region-context-menu', {
      box,
      selectionStartIndex: 0,
      selectionEndIndex: this.receivedWords.length - 1,
      text: this.getRegionText(),
    });
  }

  private onTextReceived(text: Text) {
    // Reset all old text.
    const receivedWords: Word[] = [];
    this.paragraphNumbers = [];
    this.lines = [];
    this.paragraphs = [];
    let paragraphNumber = 0;

    this.contentLanguage = text.contentLanguage ?? '';

    // If there was already text, log a text gleam render end event.
    if (this.receivedWords.length > 0) {
      recordLensOverlaySemanticEvent(SemanticEvent.kTextGleamsViewEnd);
    }

    // Flatten Text structure to a list of arrays for easier rendering and
    // referencing.
    for (const paragraph of text.textLayout?.paragraphs ?? []) {
      for (const line of paragraph.lines) {
        for (const word of line.words) {
          // Filter out words with invalid bounding boxes.
          if (isWordRenderable(word)) {
            receivedWords.push(word);
            this.paragraphNumbers.push(paragraphNumber);
          }
        }
        this.lines.push(line);
      }
      this.paragraphs.push(paragraph);
      paragraphNumber++;
    }
    // Need to set this.receivedWords to a new array instead of
    // this.receivedWords.push() to ensure the dom-repeat updates.
    this.receivedWords = receivedWords;
    // If there is text, log a text gleam render start event.
    if (this.receivedWords.length > 0) {
      recordLensOverlaySemanticEvent(SemanticEvent.kTextGleamsViewStart);
    }
    assert(this.paragraphNumbers.length === this.receivedWords.length);

    this.textReceivedTimeoutElapsedOrCleared = true;
    clearTimeout(this.textReceivedTimeoutId);
    this.textReceivedTimeoutId = -1;

    // Used to notify the post selection renderer so that, if a region has
    // already been selected, text in the region can be detected.
    this.fire('finished-receiving-text');
  }

  private getRegionText(): string {
    // Return early if there isn't a valid selection.
    if (this.receivedWords.length <= 0) {
      return '';
    }

    const startIndex = 0;
    const endIndex = this.receivedWords.length - 1;

    const selectedWords = this.receivedWords.slice(startIndex, endIndex + 1);
    const selectedParagraphNumbers =
        this.paragraphNumbers.slice(startIndex, endIndex + 1);
    return selectedWords
        .map((word, index) => {
          let separator = '';
          if (index < selectedWords.length - 1) {
            if (selectedParagraphNumbers[index] !==
                selectedParagraphNumbers[index + 1]) {
              separator = '\r\n';
            } else {
              separator = getTextSeparator(word);
            }
          }
          return word.plainText + separator;
        })
        .join('');
  }

  getElementForTesting(): Element {
    return this;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'lens-simplified-text-layer': SimplifiedTextLayerElement;
  }
}

customElements.define(
    SimplifiedTextLayerElement.is, SimplifiedTextLayerElement);
