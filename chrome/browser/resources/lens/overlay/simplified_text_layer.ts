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
import {findWordsInRegion} from './find_words_in_region.js';
import type {CenterRotatedBox} from './geometry.mojom-webui.js';
import {SemanticEvent} from './lens.mojom-webui.js';
import {recordLensOverlaySemanticEvent} from './metrics_utils.js';
import type {GestureEvent} from './selection_utils.js';
import {getCss} from './simplified_text_layer.css.js';
import {getHtml} from './simplified_text_layer.html.js';
import type {Text} from './text.mojom-webui.js';
import {createHighlightedLines, type HighlightedLine} from './text_highlights.js';
import type {TextCopyCallback, TextLayerBase} from './text_layer_base.js';
import {getTextSeparator, isWordRenderable, type TextResponse, translateWords} from './text_rendering.js';
import {toPercent} from './values_converter.js';

// A struct for holding the ID of a timeout and whether it has elapsed since
// being set. Needed to clear the timeout.
interface Timeout {
  timeout: number;
  timeoutId: number;
  timeoutElapsedOrCleared: boolean;
}

/*
 * Element responsible for highlighting and selection text.
 */
export class SimplifiedTextLayerElement extends CrLitElement implements
    TextLayerBase {
  static get is() {
    return 'lens-simplified-text-layer';
  }

  static override get properties() {
    return {
      hasActionedText: {
        type: Boolean,
        reflect: true,
      },
      hideHighlightedLines: {
        type: Boolean,
        reflect: true,
      },
      highlightedLines: {type: Array},
    };
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  // Whether the user has actioned on the text pertaining to the newest region
  // selection made either by attempting to copy or translate.
  protected accessor hasActionedText: boolean = false;
  // Whether to hide the highlighted lines in the region. Starts off true so
  // highlighted lines can initially fade in.
  protected accessor hideHighlightedLines: boolean = true;
  // The currently selected lines.
  protected accessor highlightedLines: HighlightedLine[] = [];

  // The lens text response corresponding to the full image response.
  private fullTextResponse: TextResponse|null = null;
  // The Lens text response corresponding to the regions selected.
  private regionTextResponse: TextResponse|null = null;
  private eventTracker_: EventTracker = new EventTracker();
  // The bounds of the parent element. This is updated by the parent to avoid
  // this class needing to call getBoundingClientRect()
  private selectionOverlayRect: DOMRect;
  private listenerIds: number[] = [];
  // Whether the user is in the middle of selecting a new region.
  private isSelectingRegion: boolean = false;
  // Whether to send an event to the parent selection overlay to show the
  // context menu after detecting text in a region. Set to false if the context
  // menu was already shown.
  private shouldShowContextMenuIfDetectsText: boolean = true;
  // Timeout for onTextReceived for the full image response text. The selected
  // region context menu should not be shown until either the text is received
  // or the timeout elapses.      ;
  private textReceivedTimeout: Timeout = {
    timeout: loadTimeData.getInteger('textReceivedTimeout'),
    timeoutId: -1,
    timeoutElapsedOrCleared: false,
  };
  // Timeout for using the full image text response in response to a copy text
  // context menu action.
  private copyTextTimeout: Timeout = {
    timeout: loadTimeData.getInteger('copyTextTimeout'),
    timeoutId: -1,
    timeoutElapsedOrCleared: false,
  };
  // A callback provided by the parent for copying the text on the overlay.
  private copyTextFunction: TextCopyCallback;
  // Timeout for using the full image text response in response to a translate
  // context menu action.
  private translateTimeout: Timeout = {
    timeout: loadTimeData.getInteger('translateTextTimeout'),
    timeoutId: -1,
    timeoutElapsedOrCleared: false,
  };

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
      this.browserProxy.callbackRouter.clearAllSelections.addListener(
          this.onClearRegionSelection.bind(this)),
      this.browserProxy.callbackRouter.clearRegionSelection.addListener(
          this.onClearRegionSelection.bind(this)),
    ];

    this.setTextReceivedTimeout();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    // If there was rendered text, log a text gleam render end event.
    if (this.regionTextResponse &&
        this.regionTextResponse.receivedWords.length > 0) {
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
    this.isSelectingRegion = true;
    this.hasActionedText = false;
    this.shouldShowContextMenuIfDetectsText = true;
    // Hide highlighted lines but do not clear them in order to allow them to
    // fade out.
    this.hideHighlightedLines = true;
    this.fire('hide-selected-region-context-menu');
  }

  onSelectionFinish(): void {
    this.isSelectingRegion = false;
    // Clear the previous region selection text response as a new selection has
    // been made. Also clear any timeouts that also pertained to the last region
    // response.
    this.regionTextResponse = null;
    this.clearTextTimeouts();
  }

  selectAndSendWords(_selectionStartIndex: number, _selectionEndIndex: number) {
    // Do nothing. The simplified text layer does not support selecting words.
  }

  selectAndTranslateWords(startIndex: number, endIndex: number) {
    if (this.regionTextResponse) {
      this.hasActionedText = true;
      translateWords(
          this.getRegionText(), this.regionTextResponse.contentLanguage, 0,
          this.regionTextResponse.receivedWords.length - 1, this.browserProxy);
      return;
    }

    assert(this.fullTextResponse);
    if (this.translateTimeout.timeoutElapsedOrCleared) {
      this.hasActionedText = true;
      // This layer does not support selection of text. So just translate the
      // words.
      translateWords(
          this.getRegionTextFromFullImage(startIndex, endIndex),
          this.fullTextResponse.contentLanguage, startIndex, endIndex,
          this.browserProxy);
      this.highlightedLines =
          createHighlightedLines(this.fullTextResponse, startIndex, endIndex);
      this.hideHighlightedLines = false;
      return;
    }

    this.translateTimeout.timeoutId = setTimeout(() => {
      this.translateTimeout.timeoutElapsedOrCleared = true;
      this.translateTimeout.timeoutId = -1;
      this.selectAndTranslateWords(startIndex, endIndex);
    }, this.translateTimeout.timeout);
  }

  private onClearRegionSelection() {
    this.isSelectingRegion = false;
    this.hasActionedText = false;
    this.hideHighlightedLines = true;
    this.highlightedLines = [];
  }

  private setTextReceivedTimeout() {
    this.textReceivedTimeout.timeoutElapsedOrCleared = false;
    this.textReceivedTimeout.timeoutId = setTimeout(() => {
      this.textReceivedTimeout.timeoutElapsedOrCleared = true;
      this.textReceivedTimeout.timeoutId = -1;
    }, this.textReceivedTimeout.timeout);
  }

  private detectTextInRegion(box: CenterRotatedBox) {
    // If we are still waiting for the text, hide the context menu.
    if (!this.textReceivedTimeout.timeoutElapsedOrCleared) {
      this.fire('hide-selected-region-context-menu');
      return;
    }

    const showOrUpdateEventName = this.shouldShowContextMenuIfDetectsText ?
        'show-selected-region-context-menu' :
        'update-selected-region-context-menu';
    // Only ever show the region context menu once per region selection. All
    // other times it should only update the context menu.
    this.shouldShowContextMenuIfDetectsText = false;
    // If there is region text in the interaction response,
    if (this.regionTextResponse) {
      if (this.regionTextResponse.receivedWords.length > 0) {
        this.fire(showOrUpdateEventName, {
          box,
          selectionStartIndex: 0,
          selectionEndIndex: this.regionTextResponse.receivedWords.length - 1,
          text: this.getRegionText(),
        });
        return;
      }

      // Do not show the detected text context menu items if there was no text
      // found in the region.
      this.fire(showOrUpdateEventName, {
        box,
        selectionStartIndex: -1,
        selectionEndIndex: -1,
      });
      return;
    }

    if (this.fullTextResponse &&
        this.fullTextResponse.receivedWords.length > 0) {
      const selection = findWordsInRegion(
          this.fullTextResponse.receivedWords, box, this.selectionOverlayRect);
      // Words may be found in the region even if the IOU threshold is not met.
      // If IOU threshold is not met, behave as if no words were found. Show the
      // context menu but do not send the selection indices so that options for
      // detected text are not shown.
      if (selection.iou > 0.1) {
        this.fire(showOrUpdateEventName, {
          box,
          selectionStartIndex: selection.startIndex,
          selectionEndIndex: selection.endIndex,
          text: this.getRegionTextFromFullImage(
              selection.startIndex, selection.endIndex),
        });
        return;
      }
    }

    // Do not show the detected text context menu items if there was no text
    // found in the region.
    this.fire(showOrUpdateEventName, {
      box,
      selectionStartIndex: -1,
      selectionEndIndex: -1,
    });
  }

  private onRegionTextReceived(text: Text) {
    // If the user is currently selecting a new region, ignore any text received
    // for the old region.
    if (this.isSelectingRegion) {
      return;
    }

    // If there was rendered text, log a text gleam render end event.
    if (this.regionTextResponse &&
        this.regionTextResponse.receivedWords.length > 0) {
      recordLensOverlaySemanticEvent(SemanticEvent.kTextGleamsViewEnd);
    }

    // Reset all old text.
    this.regionTextResponse = {
      contentLanguage: '',
      receivedWords: [],
      paragraphNumbers: [],
      lineNumbers: [],
      lines: [],
      paragraphs: [],
    };
    let lineNumber = 0;
    let paragraphNumber = 0;

    // Flatten Text structure to a list of arrays for easier rendering and
    // referencing.
    for (const paragraph of text.textLayout?.paragraphs ?? []) {
      for (const line of paragraph.lines) {
        for (const word of line.words) {
          // Filter out words with invalid bounding boxes.
          if (isWordRenderable(word)) {
            this.regionTextResponse.receivedWords.push(word);
            this.regionTextResponse.lineNumbers.push(lineNumber);
            this.regionTextResponse.paragraphNumbers.push(paragraphNumber);

            const wordBoundingBox = word.geometry?.boundingBox;
            assert(wordBoundingBox);
          }
        }
        this.regionTextResponse.lines.push(line);
        lineNumber++;
      }
      this.regionTextResponse.paragraphs.push(paragraph);
      paragraphNumber++;
    }
    // If there is text, log a text gleam render start event.
    if (this.regionTextResponse.receivedWords.length > 0) {
      recordLensOverlaySemanticEvent(SemanticEvent.kTextGleamsViewStart);
    }
    assert(
        this.regionTextResponse.paragraphNumbers.length ===
        this.regionTextResponse.receivedWords.length);

    this.highlightedLines = createHighlightedLines(
        this.regionTextResponse, 0,
        this.regionTextResponse.receivedWords.length - 1);
    this.hideHighlightedLines = false;

    // Used to notify the post selection renderer so that, if a region has
    // already been selected, text in the region can be detected.
    this.fire('finished-receiving-text');
    if (this.copyTextTimeout.timeoutId !== -1) {
      this.copyTextTimeout.timeoutElapsedOrCleared = true;
      clearTimeout(this.copyTextTimeout.timeoutId);
      this.copyTextTimeout.timeoutId = -1;
      this.onCopyDetectedText(
          0, this.regionTextResponse.receivedWords.length - 1,
          this.copyTextFunction);
    }

    if (this.translateTimeout.timeoutId !== -1) {
      this.translateTimeout.timeoutElapsedOrCleared = true;
      clearTimeout(this.translateTimeout.timeoutId);
      this.translateTimeout.timeoutId = -1;
      this.selectAndTranslateWords(
          0, this.regionTextResponse.receivedWords.length - 1);
    }
  }

  private onTextReceived(text: Text) {
    if (this.fullTextResponse) {
      this.onRegionTextReceived(text);
      return;
    }

    // Reset all old text.
    this.fullTextResponse = {
      contentLanguage: '',
      receivedWords: [],
      paragraphNumbers: [],
      lineNumbers: [],
      lines: [],
      paragraphs: [],
    };
    let paragraphNumber = 0;

    // Flatten Text structure to a list of arrays for easier rendering and
    // referencing.
    for (const paragraph of text.textLayout?.paragraphs ?? []) {
      for (const line of paragraph.lines) {
        for (const word of line.words) {
          // Filter out words with invalid bounding boxes.
          if (isWordRenderable(word)) {
            this.fullTextResponse.receivedWords.push(word);
            this.fullTextResponse.paragraphNumbers.push(paragraphNumber);
          }
        }
        this.fullTextResponse.lines.push(line);
      }
      this.fullTextResponse.paragraphs.push(paragraph);
      paragraphNumber++;
    }
    assert(
        this.fullTextResponse.paragraphNumbers.length ===
        this.fullTextResponse.receivedWords.length);

    this.textReceivedTimeout.timeoutElapsedOrCleared = true;
    clearTimeout(this.textReceivedTimeout.timeoutId);
    this.textReceivedTimeout.timeoutId = -1;

    // Used to notify the post selection renderer so that, if a region has
    // already been selected, text in the region can be detected.
    this.fire('finished-receiving-text');
  }

  onCopyDetectedText(
      startIndex: number, endIndex: number, callback: TextCopyCallback) {
    if (startIndex < 0 || endIndex < 0) {
      return;
    }
    this.hasActionedText = true;

    if (this.regionTextResponse) {
      callback(/*textStartIndex=*/ 0,
               this.regionTextResponse.receivedWords.length - 1,
               this.getRegionText());
      return;
    }

    // If no region text, set a timeout to wait a little longer in case it has
    // not returned yet. After that timeout is over, the text will be copied
    // from the full image.
    if (this.copyTextTimeout.timeoutElapsedOrCleared) {
      // If the full image text response was never returned, then do nothing.
      // This is possible if the full image request fails and the user uses
      // CMD+C to copy.
      if (this.fullTextResponse) {
        callback(
            startIndex, endIndex,
            this.getRegionTextFromFullImage(startIndex, endIndex));

        this.highlightedLines =
            createHighlightedLines(this.fullTextResponse, startIndex, endIndex);
        this.hideHighlightedLines = false;
      }
      return;
    }

    // Since the copy command can be spammed, make sure the timeout is only set
    // once.
    if (this.copyTextTimeout.timeoutId === -1) {
      this.copyTextFunction = callback;
      this.copyTextTimeout.timeoutId = setTimeout(() => {
        this.copyTextTimeout.timeoutElapsedOrCleared = true;
        this.copyTextTimeout.timeoutId = -1;
        this.onCopyDetectedText(startIndex, endIndex, callback);
      }, this.copyTextTimeout.timeout);
    }
  }

  private getRegionTextFromFullImage(
      textStartIndex: number, textEndIndex: number): string {
    // Return early if there isn't a valid range.
    if (textStartIndex === -1 || textEndIndex === -1 ||
        !this.fullTextResponse) {
      return '';
    }

    const startIndex = Math.min(textStartIndex, textEndIndex);
    const endIndex = Math.max(textStartIndex, textEndIndex);
    const selectedWords =
        this.fullTextResponse.receivedWords.slice(startIndex, endIndex + 1);
    const selectedParagraphNumbers =
        this.fullTextResponse.paragraphNumbers.slice(startIndex, endIndex + 1);
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

  private getRegionText(): string {
    // Return early if there isn't any text in the region.
    if (!this.regionTextResponse ||
        this.regionTextResponse.receivedWords.length <= 0) {
      return '';
    }

    const startIndex = 0;
    const endIndex = this.regionTextResponse.receivedWords.length - 1;

    const selectedWords =
        this.regionTextResponse.receivedWords.slice(startIndex, endIndex + 1);
    const selectedParagraphNumbers =
        this.regionTextResponse.paragraphNumbers.slice(
            startIndex, endIndex + 1);
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

  /** @return The CSS styles string for the given highlighted line. */
  protected getHighlightedLineStyle(line: HighlightedLine): string {
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

  private clearTextTimeouts() {
    clearTimeout(this.copyTextTimeout.timeoutId);
    this.copyTextTimeout.timeoutElapsedOrCleared = false;
    this.copyTextTimeout.timeoutId = -1;
    clearTimeout(this.translateTimeout.timeoutId);
    this.translateTimeout.timeoutElapsedOrCleared = false;
    this.translateTimeout.timeoutId = -1;
  }

  getElementForTesting(): Element {
    return this;
  }

  getHasActionedTextForTesting(): boolean {
    return this.hasActionedText;
  }

  setSelectionOverlayRectForTesting(rect: DOMRect): void {
    this.selectionOverlayRect = rect;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'lens-simplified-text-layer': SimplifiedTextLayerElement;
  }
}

customElements.define(
    SimplifiedTextLayerElement.is, SimplifiedTextLayerElement);
