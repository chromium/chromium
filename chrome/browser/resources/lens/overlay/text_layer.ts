// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import {assert} from '//resources/js/assert.js';
import {skColorToHexColor, skColorToRgba} from '//resources/js/color_utils.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {PointF} from '//resources/mojo/ui/gfx/geometry/mojom/geometry.mojom-webui.js';
import {afterNextRender, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {DomRepeat} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import type {BrowserProxy} from './browser_proxy.js';
import {skColorToRgbaWithCustomAlpha} from './color_utils.js';
import {type CursorTooltipData, CursorTooltipType} from './cursor_tooltip.js';
import {findWordsInRegion} from './find_words_in_region.js';
import {CenterRotatedBox_CoordinateType} from './geometry.mojom-webui.js';
import type {CenterRotatedBox} from './geometry.mojom-webui.js';
import {bestHit} from './hit.js';
import {SemanticEvent, UserAction} from './lens.mojom-webui.js';
import {INVOCATION_SOURCE} from './lens_overlay_app.js';
import {recordLensOverlayInteraction, recordLensOverlaySemanticEvent} from './metrics_utils.js';
import type {CursorData, SelectedRegionContextMenuData, SelectedTextContextMenuData} from './selection_overlay.js';
import {CursorType} from './selection_utils.js';
import type {GestureEvent} from './selection_utils.js';
import type {BackgroundImageData, Line, Paragraph, Text, TranslatedLine, TranslatedParagraph, Word} from './text.mojom-webui.js';
import {Alignment, WritingDirection} from './text.mojom-webui.js';
import {getTemplate} from './text_layer.html.js';
import type {TranslateState} from './translate_button.js';
import {toPercent} from './values_converter.js';

// Lowest font size that translate text can be rendered at in pixels.
const MIN_FONT_SIZE = 1;
// Largest font size that translate text can be rendered at in pixels.
const MAX_FONT_SIZE = 100;
// Highest font size where the opacity of the background should be 100%.
const FONT_SIZE_OPAQUE_BOUND = 10;
// Lowest font size where the opacity of the background should be transparent
const FONT_SIZE_TRANSPARENT_BOUND = 18;

// The language codes that are considered RTL languages as used in Lens.
const RTL_LANGUAGES = new Set([
  'ar' /* Arabic */,
  'bal' /* Baluchi */,
  'bm-Nkoo' /* Nko */,
  'ckb' /* Kurdish (Sorani) */,
  'dv' /* Divehi */,
  'fa' /* Persian */,
  'fa-AF' /* Dari */,
  'he' /* Hebrew */,
  'iw' /* Hebrew synonym */,
  'ji' /* Yiddish synonym */,
  'ms-Arab' /* Malay (Jawi) */,
  'ks' /* Kashmiri */,
  'pa-Arab', /* Punjabi (Shahmukhi) */
  'ps' /* Pashto */,
  'sd' /* Sindhi */,
  'ug' /* Uighur */,
  'ur' /* Urdu */,
  'yi' /* Yiddish */,
]);

// Returns whether the provided language code is an RTL language.
function isRtlLanguage(languageCode: string) {
  return RTL_LANGUAGES.has(languageCode);
}

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
  return (word.textSeparator !== null && word.textSeparator !== undefined) ?
      word.textSeparator :
      ' ';
}

// Returns true if index is in the range [start, end]. End index may be lesser
// than start index.
function isInRange(index: number, start: number, end: number): boolean {
  return (index >= start && index <= end) || (index >= end && index <= start);
}

export interface TextLayerElement {
  $: {
    textRenderCanvas: HTMLCanvasElement,
    translateContainer: DomRepeat,
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

interface TranslatedLineData {
  alignment: Alignment;
  contentLanguage: string;
  line: TranslatedLine;
  words: TranslatedWordData[];
  paragraphIndex: number;
}

interface TranslatedWordData {
  word: Word;
  index: number;
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
      shouldRenderTranslateWords: {
        type: Boolean,
        reflectToAttribute: true,
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
      selectionOverlayRect: {
        type: Object,
        observer: 'computeTranslatedWordBoundingBoxes',
      },
    };
  }

  // The rendering context of the canvas used to measure font size of translated
  // text.
  private context: CanvasRenderingContext2D;
  // The words rendered in this layer.
  private renderedWords: Word[];
  // Whether to render the translated text received on the overlay rather than
  // the detected text.
  private shouldRenderTranslateWords: boolean;
  // The current target language the user requested to translate to.
  private currentTranslateLanguage: string;
  // All of the translated words returned in OnTextReceived with failed
  // translations replaced with their non-translated counterpart.
  private renderedTranslateWords: Word[];
  // The rendered translated lines in order from OnTextReceived.
  private renderedTranslateLines: TranslatedLineData[];
  // The rendered translated paragraphs keyed by the paragraph number.
  private renderedTranslateParagraphs:
      {[paragraphNumber: number]: TranslatedParagraph};
  // The detected words that did not have translations. Keyed by the word index
  // used when rendering only detected words. This allows us to use the same
  // detected words when rendering the translated text.
  private detectedWordToTranslateIndex: {[detectedWordIndex: number]: number};
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
  // The bounds of the parent element. This is updated by the parent to avoid
  // this class needing to call getBoundingClientRect()
  private selectionOverlayRect: DOMRect;

  // An array that corresponds 1:1 to renderedWords, where lineNumbers[i] is the
  // line number for renderedWords[i]. In addition, the index at lineNumbers[i]
  // corresponds to the Line in lines[i] that the word belongs in.
  private lineNumbers: number[];
  // An array that corresponds 1:1 to renderedWords, where paragraphNumbers[i]
  // is the paragraph number for renderedWords[i]. In addition, the index at
  // paragraphNumbers[i] corresponds to the Paragraph in paragraphs[i] that the
  // word belongs in.
  private paragraphNumbers: number[];
  // An array that corresponds 1:1 to renderedTranslateWords, where
  // translatedLineNumbers[i] is the line number for renderedTranslateWords[i].
  // In addition, the index at translatedLineNumbers[i] corresponds to the Line
  // in lines[i] that the word belongs in.
  private translatedLineNumbers: number[];
  // An array that corresponds 1:1 to renderedTranslateWords, where
  // translatedParagraphNumbers[i] is the line number for
  // renderedTranslateWords[i]. In addition, the index at
  // translatedParagraphNumbers[i] corresponds to the Line in lines[i] that the
  // word belongs in.
  private translatedParagraphNumbers: number[];
  // The lines received from OnTextReceived.
  private lines: Line[];
  // The paragraphs received from OnTextReceived.
  private paragraphs: Paragraph[];
  // The content language received from OnTextReceived.
  private contentLanguage: string;
  private eventTracker_: EventTracker = new EventTracker();
  private listenerIds: number[];
  // IoU threshold for finding words in region.
  private selectTextTriggerThreshold: number =
      loadTimeData.getValue('selectTextTriggerThreshold');
  // Timeout for onTextReceived. We do not want to show the selected region
  // context menu until either the text is received or the timeout elapses.
  private textReceivedTimeout: number =
      loadTimeData.getValue('textReceivedTimeout');
  private textReceivedTimeoutID: number = 0;
  private textReceivedTimeoutElapsedOrCleared = false;
  private browserProxy: BrowserProxy = BrowserProxyImpl.getInstance();

  override ready() {
    super.ready();
    this.context = this.$.textRenderCanvas.getContext('2d')!;
  }

  override connectedCallback() {
    super.connectedCallback();

    this.eventTracker_.add(
        document, 'detect-text-in-region',
        (e: CustomEvent<CenterRotatedBox>) => {
          this.detectTextInRegion(e.detail);
        });
    this.eventTracker_.add(
        document, 'translate-mode-state-changed',
        (e: CustomEvent<TranslateState>) => {
          this.shouldRenderTranslateWords = e.detail.translateModeEnabled;
          this.currentTranslateLanguage = e.detail.targetLanguage;
          if (e.detail.shouldUnselectWords) {
            this.unselectWords();
          }
        });

    // Set up listener to listen to events from C++.
    this.listenerIds = [
      this.browserProxy.callbackRouter.textReceived.addListener(
          this.onTextReceived.bind(this)),
      this.browserProxy.callbackRouter.clearTextSelection.addListener(
          this.unselectWords.bind(this)),
      this.browserProxy.callbackRouter.clearAllSelections.addListener(
          this.unselectWords.bind(this)),
      this.browserProxy.callbackRouter.setTextSelection.addListener(
          this.selectWords.bind(this)),
    ];

    this.textReceivedTimeoutID = setTimeout(() => {
      this.textReceivedTimeoutElapsedOrCleared = true;
    }, this.textReceivedTimeout);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    // If there was rendered text, log a text gleam render end event.
    if (this.renderedWords?.length > 0) {
      recordLensOverlaySemanticEvent(SemanticEvent.kTextGleamsViewEnd);
    }

    this.listenerIds.forEach(
        id => assert(this.browserProxy.callbackRouter.removeListener(id)));
    this.listenerIds = [];
    this.eventTracker_.removeAll();
  }

  private handlePointerEnter() {
    this.dispatchEvent(new CustomEvent<CursorData>(
        'set-cursor',
        {bubbles: true, composed: true, detail: {cursor: CursorType.TEXT}}));
    this.dispatchEvent(
        new CustomEvent<CursorTooltipData>('set-cursor-tooltip', {
          bubbles: true,
          composed: true,
          detail: {tooltipType: CursorTooltipType.TEXT_HIGHLIGHT},
        }));
  }

  private handlePointerLeave() {
    if (this.shouldRenderTranslateWords) {
      // In translate mode, always allow text selection from anywhere.
      return;
    }
    this.dispatchEvent(new CustomEvent<CursorData>(
        'set-cursor',
        {bubbles: true, composed: true, detail: {cursor: CursorType.DEFAULT}}));
    this.dispatchEvent(
        new CustomEvent<CursorTooltipData>('set-cursor-tooltip', {
          bubbles: true,
          composed: true,
          detail: {tooltipType: CursorTooltipType.REGION_SEARCH},
        }));
  }

  private detectTextInRegion(box: CenterRotatedBox) {
    // If we are still waiting for the text, hide the context menu.
    if (!this.textReceivedTimeoutElapsedOrCleared) {
      this.dispatchEvent(new CustomEvent(
          'hide-selected-region-context-menu',
          {bubbles: true, composed: true}));
      return;
    }

    const selection =
        findWordsInRegion(this.renderedWords, box, this.selectionOverlayRect);
    // Words may be found in the region even if the IOU threshold is not met.
    // If IOU threshold is not met, behave as if no words were found. Show the
    // context menu but do not send the selection indices so that options for
    // detected text are not shown.
    if (selection.iou < this.selectTextTriggerThreshold) {
      this.dispatchEvent(new CustomEvent<SelectedRegionContextMenuData>(
          'show-selected-region-context-menu', {
            bubbles: true,
            composed: true,
            detail: {box, selectionStartIndex: -1, selectionEndIndex: -1},
          }));
      return;
    }

    this.dispatchEvent(new CustomEvent<SelectedRegionContextMenuData>(
        'show-selected-region-context-menu', {
          bubbles: true,
          composed: true,
          detail: {
            box,
            selectionStartIndex: selection.startIndex,
            selectionEndIndex: selection.endIndex,
          },
        }));
  }

  handleGestureStart(event: GestureEvent): boolean {
    this.unselectWords();

    const translatedWordIndex =
        this.translatedWordIndexFromPoint(event.startX, event.startY);
    let wordIndex = translatedWordIndex !== null ?
        translatedWordIndex :
        this.wordIndexFromPoint(event.startX, event.startY);
    if (wordIndex === null && this.shouldRenderTranslateWords) {
      // If translate mode is enabled, selecting text should work anywhere, so
      // select the closest word if the cursor was not actually on top of a
      // word.
      const imageBounds = this.selectionOverlayRect;
      const normalizedX = (event.startX - imageBounds.left) / imageBounds.width;
      const normalizedY = (event.startY - imageBounds.top) / imageBounds.height;
      const hit = bestHit(
          this.renderedTranslateWords, {x: normalizedX, y: normalizedY});
      if (hit) {
        wordIndex = this.renderedTranslateWords.indexOf(hit);
      }
    }
    // Ignore if the click is not on a word.
    if (wordIndex === null) {
      return false;
    }

    this.selectionStartIndex = wordIndex;
    this.selectionEndIndex = wordIndex;
    this.isSelectingText = true;
    return true;
  }

  handleRightClick(event: PointerEvent): boolean {
    // If the user right-clicks a highlighted word, restore the selected text
    // context menu.
    const translatedWordIndex =
        this.translatedWordIndexFromPoint(event.clientX, event.clientY);
    const wordIndex = translatedWordIndex !== null ?
        translatedWordIndex :
        this.wordIndexFromPoint(event.clientX, event.clientY);
    if (wordIndex !== null &&
        isInRange(
            wordIndex, this.selectionStartIndex, this.selectionEndIndex)) {
      this.dispatchEvent(new CustomEvent('restore-selected-text-context-menu', {
        bubbles: true,
        composed: true,
      }));
      return true;
    }
    return false;
  }

  handleGestureDrag(event: GestureEvent) {
    const imageBounds = this.selectionOverlayRect;
    const normalizedX = (event.clientX - imageBounds.left) / imageBounds.width;
    const normalizedY = (event.clientY - imageBounds.top) / imageBounds.height;

    const words = this.shouldRenderTranslateWords ?
        this.renderedTranslateWords :
        this.renderedWords;
    const hit = bestHit(words, {x: normalizedX, y: normalizedY});

    if (!hit) {
      return;
    }

    this.selectionEndIndex = words.indexOf(hit);
  }

  handleGestureEnd() {
    this.sendSelectedText();
  }

  private computeTranslatedWordBoundingBoxes() {
    // Return early if we are not in translate mode or there are no rendered
    // translate words.
    if (!this.shouldRenderTranslateWords ||
        !(this.renderedTranslateLines.length > 0) ||
        !(this.renderedTranslateWords.length > 0)) {
      return;
    }

    const wordSpanElements = this.shadowRoot!.querySelectorAll<HTMLSpanElement>(
        'span[data-word-index]');
    for (const wordSpanElement of wordSpanElements) {
      const wordIndexString = wordSpanElement.dataset['wordIndex'];
      const lineIndexString = wordSpanElement.dataset['lineIndex'];
      // The word index is guaranteed to exist because of the query selector.
      assert(wordIndexString);
      assert(lineIndexString);
      const wordIndex = parseInt(wordIndexString) ?? -1;
      const lineIndex = parseInt(lineIndexString) ?? -1;
      // The word index should always be parseable as a positive number since we
      // create it as one.
      assert(wordIndex >= 0);
      assert(lineIndex >= 0);
      const word = this.renderedTranslateWords[wordIndex];
      const translatedLine = this.renderedTranslateLines[lineIndex];

      // Create the geometry and bounding box for the word from the span
      // element.
      const boundingRect = wordSpanElement.getBoundingClientRect();
      const centerX = boundingRect.left - this.selectionOverlayRect.left +
          boundingRect.width / 2;
      const centerY = boundingRect.top - this.selectionOverlayRect.top +
          boundingRect.height / 2;

      const normalizedCenterX = centerX / this.selectionOverlayRect.width;
      const normalizedCenterY = centerY / this.selectionOverlayRect.height;
      const normalizedWidth =
          wordSpanElement.offsetWidth / this.selectionOverlayRect.width;
      const normalizedHeight =
          wordSpanElement.offsetHeight / this.selectionOverlayRect.height;
      assert(translatedLine.line.geometry);
      const rotation = translatedLine.line.geometry.boundingBox.rotation;

      const rect = {
        x: normalizedCenterX,
        y: normalizedCenterY,
        width: normalizedWidth,
        height: normalizedHeight,
      };
      const centerRotatedBox = {
        box: rect,
        rotation,
        coordinateType: CenterRotatedBox_CoordinateType.kNormalized,
      };
      const geometry = {boundingBox: centerRotatedBox, segmentationPolygon: []};
      word.geometry = geometry;
    }
  }

  private sendSelectedText() {
    this.isSelectingText = false;
    const highlightedText = this.getHighlightedText();
    const lines = this.getHighlightedLines();
    const containingRect = this.getContainingRect(lines);
    this.dispatchEvent(new CustomEvent<SelectedTextContextMenuData>(
        'show-selected-text-context-menu', {
          bubbles: true,
          composed: true,
          detail: {
            text: highlightedText,
            contentLanguage: this.contentLanguage,
            left: containingRect.left,
            right: containingRect.right,
            top: containingRect.top,
            bottom: containingRect.bottom,
            selectionStartIndex: this.selectionStartIndex,
            selectionEndIndex: this.selectionEndIndex,
          },
        }));

    // On selection complete, send the selected text to C++.
    this.browserProxy.handler.issueTextSelectionRequest(
        highlightedText, this.selectionStartIndex, this.selectionEndIndex);
    recordLensOverlayInteraction(
        INVOCATION_SOURCE,
        this.shouldRenderTranslateWords ? UserAction.kTranslateTextSelection :
                                          UserAction.kTextSelection);
  }

  selectAndSendWords(selectionStartIndex: number, selectionEndIndex: number) {
    this.selectWords(selectionStartIndex, selectionEndIndex);
    this.sendSelectedText();
  }

  selectAndTranslateWords(
      selectionStartIndex: number, selectionEndIndex: number) {
    this.selectWords(selectionStartIndex, selectionEndIndex);
    this.isSelectingText = false;
    // Do not show the selected text context menu, but update the data so that
    // it is shown correctly if the user right-clicks on the text.
    const highlightedText = this.getHighlightedText();
    const lines = this.getHighlightedLines();
    const containingRect = this.getContainingRect(lines);
    this.dispatchEvent(new CustomEvent<SelectedTextContextMenuData>(
        'update-selected-text-context-menu', {
          bubbles: true,
          composed: true,
          detail: {
            text: highlightedText,
            contentLanguage: this.contentLanguage,
            left: containingRect.left,
            right: containingRect.right,
            top: containingRect.top,
            bottom: containingRect.bottom,
            selectionStartIndex: this.selectionStartIndex,
            selectionEndIndex: this.selectionEndIndex,
          },
        }));

    BrowserProxyImpl.getInstance().handler.issueTranslateSelectionRequest(
        this.getHighlightedText(), this.contentLanguage,
        this.selectionStartIndex, this.selectionEndIndex);
    recordLensOverlayInteraction(INVOCATION_SOURCE, UserAction.kTranslateText);
  }

  cancelGesture() {
    this.unselectWords();
  }

  private unselectWords() {
    this.selectionStartIndex = -1;
    this.selectionEndIndex = -1;
    this.dispatchEvent(new CustomEvent(
        'hide-selected-text-context-menu', {bubbles: true, composed: true}));
    this.dispatchEvent(new CustomEvent(
        'hide-selected-region-context-menu', {bubbles: true, composed: true}));
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
    this.contentLanguage = text.contentLanguage ?? '';
    let lineNumber = 0;
    let paragraphNumber = 0;

    // If there was already text, log a text gleam render end event.
    if (this.renderedWords?.length > 0) {
      recordLensOverlaySemanticEvent(SemanticEvent.kTextGleamsViewEnd);
    }

    // Reset all old translation text.
    let detectedWordIndex = 0;
    let translatedWordIndex = 0;
    let translatedLineNumber = 0;
    const receivedTranslateLines = [];
    this.translatedLineNumbers = [];
    this.translatedParagraphNumbers = [];
    this.renderedTranslateWords = [];
    this.renderedTranslateLines = [];
    this.renderedTranslateParagraphs = {};
    this.detectedWordToTranslateIndex = {};

    // Flatten Text structure to a list of arrays for easier rendering and
    // referencing.
    for (const paragraph of text.textLayout.paragraphs) {
      const hasParagraphTranslation = paragraph.translation !== null;
      // We are looking for translated paragraphs first. If they do not exist,
      // we should default to the detected text. Just because we have
      // translations for some paragraphs does not mean we have translations
      // for all paragraphs.
      if (hasParagraphTranslation) {
        // Assert the paragraph translation so the linter does not complain.
        assert(paragraph.translation !== null);
        for (const line of paragraph.translation.lines) {
          const translatedWordDataInLine = [];
          for (const word of line.words) {
            // We do not filter out words here since the bounding boxes are
            // calculated by us in the WebUI.
            const translatedWordData:
                TranslatedWordData = {word, index: translatedWordIndex};
            this.renderedTranslateWords.push(word);
            translatedWordDataInLine.push(translatedWordData);
            this.translatedLineNumbers.push(translatedLineNumber);
            this.translatedParagraphNumbers.push(paragraphNumber);
            translatedWordIndex++;
          }

          const translatedLineData: TranslatedLineData = {
            alignment: paragraph.translation.alignment ??
                Alignment.kDefaultLeftAlgined,
            contentLanguage: paragraph.contentLanguage ?? '',
            line,
            words: translatedWordDataInLine,
            paragraphIndex: paragraphNumber,
          };
          receivedTranslateLines.push(translatedLineData);
          translatedLineNumber++;
        }
        this.renderedTranslateParagraphs[paragraphNumber] =
            paragraph.translation;
      }

      for (const line of paragraph.lines) {
        for (const word of line.words) {
          // Filter out words with invalid bounding boxes.
          if (isWordRenderable(word)) {
            receivedWords.push(word);
            this.lineNumbers.push(lineNumber);
            this.paragraphNumbers.push(paragraphNumber);

            // If this word does not have an accompanying translation, it will
            // be displayed on the screen in translate mode. So we need to add
            // to our translation text tracking as it will still be selectable.
            if (!hasParagraphTranslation) {
              this.renderedTranslateWords.push(word);
              this.translatedLineNumbers.push(translatedLineNumber);
              this.translatedParagraphNumbers.push(paragraphNumber);
              this.detectedWordToTranslateIndex[detectedWordIndex] =
                  translatedWordIndex;
              translatedWordIndex++;
              translatedLineNumber++;
            }
            detectedWordIndex++;
          }
        }
        this.lines.push(line);
        lineNumber++;

        // If this line does not have an accompanying translation, it will be
        // displayed on the screen in translate mode. So we need to increment
        // the translated line number.
        if (!hasParagraphTranslation) {
          translatedLineNumber++;
        }
      }
      this.paragraphs.push(paragraph);
      paragraphNumber++;
    }
    // Need to set this.renderedWords to a new array instead of
    // this.renderedWords.push() to ensure the dom-repeat updates.
    this.renderedWords = receivedWords;
    // If there is text, log a text gleam render start event.
    if (this.renderedWords.length > 0) {
      recordLensOverlaySemanticEvent(SemanticEvent.kTextGleamsViewStart);
    }
    assert(this.lineNumbers.length === this.renderedWords.length);
    assert(this.paragraphNumbers.length === this.renderedWords.length);

    // Our rendered translate words length should match the number of translated
    // lines we added.
    assert(
        this.renderedTranslateWords.length ===
        this.translatedLineNumbers.length);
    // Need to set this.renderedTranslateLines to a new array instead of
    // this.renderedTranslateLines.push() to ensure the dom-repeat updates.
    this.renderedTranslateLines = receivedTranslateLines;
    // We need to compute the translated bounding boxes after the next render in
    // order to make sure the span elements are on the page.
    afterNextRender(this, () => {
      this.computeTranslatedWordBoundingBoxes();
    });

    this.textReceivedTimeoutElapsedOrCleared = true;
    clearTimeout(this.textReceivedTimeoutID);

    // Used to notify the post selection renderer so that, if a region has
    // already been selected, text in the region can be detected.
    this.dispatchEvent(new CustomEvent(
        'finished-receiving-text', {bubbles: true, composed: true}));

    // Used by the translate button to label the detected language.
    this.dispatchEvent(new CustomEvent('received-content-language', {
      bubbles: true,
      composed: true,
      detail: {contentLanguage: this.contentLanguage},
    }));
  }

  private calculateFontSizePixels(translatedLine: TranslatedLineData): number {
    const line = translatedLine.line;
    if (!line.geometry) {
      return MIN_FONT_SIZE;
    }
    // TODO(b/330183480): Currently, we are assuming that word coordinates are
    // normalized. We should still implement rendering in case this assumption
    // is ever violated.
    if (line.geometry.boundingBox.coordinateType !==
        CenterRotatedBox_CoordinateType.kNormalized) {
      return MIN_FONT_SIZE;
    }

    // Convert the normalized line geometry to pixels.
    const isTopToBottom = this.isTranslatedLineVertical(translatedLine);
    const translatedLineWidth =
        (line.geometry.boundingBox.box.width * this.selectionOverlayRect.width);
    const translatedLineHeight =
        (line.geometry.boundingBox.box.height *
         this.selectionOverlayRect.height);

    // Swap width and height if we are rendering the text vertically.
    const lineWidth =
        isTopToBottom ? translatedLineHeight : translatedLineWidth;
    const lineHeight =
        isTopToBottom ? translatedLineWidth : translatedLineHeight;

    this.$.textRenderCanvas.width = lineWidth;
    this.$.textRenderCanvas.height = lineHeight;
    this.resetCanvasPixelRatioIfNeeded();

    // The line translation can contain text that is not actually a part of this
    // particular line. Because of this, we need to loop through the words and
    // create the line string ourselves.
    let text = '';
    for (let i = 0; i < line.words.length; i++) {
      const word = line.words[i];
      text += word.plainText;
      text += getTextSeparator(word);
    }

    const fontFamily = loadTimeData.getString('fontfamilyMd');
    let low = MIN_FONT_SIZE;
    let high = MAX_FONT_SIZE;
    // Use binary search to find optimal font size.
    while (low <= high) {
      const mid = Math.floor((low + high) / 2);
      // The font families here should cover what is default used by the text in
      // the HTML.
      this.context.font = `${mid}px ${fontFamily}`;
      const textMetrics = this.context.measureText(text);

      // Check if the text fits within the container
      const textHeight = textMetrics.fontBoundingBoxAscent +
          textMetrics.fontBoundingBoxDescent;
      if (textMetrics.width >= lineWidth || textHeight >= lineHeight) {
        high = mid - 1;
      } else {
        low = mid + 1;
      }
    }
    return Math.min(low - 1, MAX_FONT_SIZE);
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

    const words = this.shouldRenderTranslateWords ?
        this.renderedTranslateWords :
        this.renderedWords;
    const lineNumbers = this.shouldRenderTranslateWords ?
        this.translatedLineNumbers :
        this.lineNumbers;
    let currentLineIndex = lineNumbers[startIndex];
    let startWord: Word = words[startIndex];
    let endWord: Word = words[startIndex];

    // Get max dimensions per line.
    for (let i = startIndex; i <= endIndex; i++) {
      if (lineNumbers[i] !== currentLineIndex) {
        // Add the line
        newHighlightedLines.push(this.calculateHighlightedLine(
            startWord, endWord, this.isTopToBottomWritingDirection(i)));

        // Save new line data.
        startWord = words[i];
        currentLineIndex = lineNumbers[i];
      }
      endWord = words[i];
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
    const paragraphNumbers = this.shouldRenderTranslateWords ?
        this.translatedParagraphNumbers :
        this.paragraphNumbers;
    const paragraph = this.paragraphs[paragraphNumbers[wordIndex]];
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

    const selectedWords = this.shouldRenderTranslateWords ?
        this.renderedTranslateWords.slice(startIndex, endIndex + 1) :
        this.renderedWords.slice(startIndex, endIndex + 1);
    return selectedWords
        .map((word, index) => {
          return word.plainText +
              (index < selectedWords.length - 1 ? getTextSeparator(word) : '');
        })
        .join('');
  }

  /** @return The CSS styles string for the given word. */
  private getWordStyle(word: Word, wordIndex: number): string {
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

    // We do not want to render this word if we are in translate mode and the
    // paragraph this word pertains to has translated text.
    const paragraph = this.paragraphs[this.paragraphNumbers[wordIndex]];
    if (this.shouldRenderTranslateWords && paragraph.translation) {
      return 'display: none;';
    }

    const horizontalLineMarginPercent =
        loadTimeData.getInteger('verticalTextMarginPx') /
        this.selectionOverlayRect.height;
    const verticalLineMarginPercent =
        loadTimeData.getInteger('horizontalTextMarginPx') /
        this.selectionOverlayRect.width;

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

  private getTranslatedLineStyle(translatedLineData: TranslatedLineData):
      string {
    const translatedLine = translatedLineData.line;
    if (!translatedLine.geometry) {
      return '';
    }

    const lineBoundingBox = translatedLine.geometry.boundingBox;
    // TODO(b/330183480): Currently, we are assuming that word
    // coordinates are normalized. We should still implement
    // rendering in case this assumption is ever violated.
    if (lineBoundingBox.coordinateType !==
        CenterRotatedBox_CoordinateType.kNormalized) {
      return '';
    }

    const lineFontSizePixels = this.calculateFontSizePixels(translatedLineData);
    const styles: string[] = [
      `background-color: ${
          this.getBackgroundColorForLine(translatedLine, lineFontSizePixels)}`,
      `color: ${skColorToHexColor(translatedLine.textColor)}`,
      `direction: ${
          this.getTranslateLanguageDirection(
              this.renderedTranslateParagraphs[translatedLineData
                                                   .paragraphIndex])}`,
      `justify-content: ${this.getLineAlignment(translatedLineData.alignment)}`,
      `font-size: ${lineFontSizePixels}px`,
      `width: ${toPercent(lineBoundingBox.box.width)}`,
      `height: ${toPercent(lineBoundingBox.box.height)}`,
      `top: ${
          toPercent(lineBoundingBox.box.y - (lineBoundingBox.box.height / 2))}`,
      `left: ${
          toPercent(lineBoundingBox.box.x - (lineBoundingBox.box.width / 2))}`,
      `text-shadow: ${
          this.getOutlineStyleForLine(translatedLine, lineFontSizePixels)}`,
      `transform: rotate(${lineBoundingBox.rotation}rad)`,
      `writing-mode: ${this.getWritingModeForLine(translatedLineData)}`,
    ];
    return styles.join(';');
  }

  private getBackgroundImageDataStyle(translatedLineData: TranslatedLineData):
      string {
    const translatedLine = translatedLineData.line;
    if (!translatedLine.geometry) {
      return '';
    }

    const lineBoundingBox = translatedLine.geometry.boundingBox;
    // TODO(b/330183480): Currently, we are assuming that word
    // coordinates are normalized. We should still implement
    // rendering in case this assumption is ever violated.
    if (lineBoundingBox.coordinateType !==
        CenterRotatedBox_CoordinateType.kNormalized) {
      return '';
    }

    const backgroundImageData = translatedLine.backgroundImageData;
    if (!backgroundImageData) {
      return '';
    }

    // Both background image padding values are relative to the line height.
    const horizontalPadding =
        backgroundImageData.horizontalPadding * lineBoundingBox.box.height;
    const verticalPadding =
        backgroundImageData.verticalPadding * lineBoundingBox.box.height;

    const styles: string[] = [
      `width: ${toPercent(lineBoundingBox.box.width + horizontalPadding)}`,
      `height: ${toPercent(lineBoundingBox.box.height + verticalPadding)}`,
      `top: ${
          toPercent(
              lineBoundingBox.box.y - (lineBoundingBox.box.height / 2) -
              (0.5 * verticalPadding))}`,
      `left: ${
          toPercent(
              lineBoundingBox.box.x - (lineBoundingBox.box.width / 2) -
              (0.5 * horizontalPadding))}`,
      `transform: rotate(${lineBoundingBox.rotation}rad)`,
    ];
    return styles.join(';');
  }

  private getOutlineStyleForLine(line: TranslatedLine, fontSize: number):
      string {
    if (!line.backgroundImageData) {
      return 'none';
    }
    const outlineColor = skColorToRgba(line.backgroundPrimaryColor);
    const outlineWidth = fontSize * 0.02;
    return `-${outlineWidth}px ${outlineWidth}px 0 ${outlineColor},
            ${outlineWidth}px ${outlineWidth}px 0 ${outlineColor},
            ${outlineWidth}px -${outlineWidth}px 0 ${outlineColor},
            -${outlineWidth}px -${outlineWidth}px 0 ${outlineColor}`;
  }

  private getBackgroundColorForLine(line: TranslatedLine, fontSize: number):
      string {
    // When background image data is present, we only want it to be opaque for
    // very small text for accessibility reasons.
    if (line.backgroundImageData && fontSize >= FONT_SIZE_TRANSPARENT_BOUND) {
      return 'transparent';
    }

    // If background image data is not present, the background should be opaque.
    // Below opaque bound, it should be fully opaque.
    if (!line.backgroundImageData ||
        (line.backgroundImageData && fontSize <= FONT_SIZE_OPAQUE_BOUND)) {
      return skColorToRgba(line.backgroundPrimaryColor);
    }

    // Font sizes between the two values should iversely interpolate over 0-255
    // for opacity.
    const opacityRatio = (fontSize - FONT_SIZE_OPAQUE_BOUND) /
        (FONT_SIZE_TRANSPARENT_BOUND - FONT_SIZE_OPAQUE_BOUND);
    const clampedOpacity = Math.min(Math.max(opacityRatio, 0), 1);
    return skColorToRgbaWithCustomAlpha(
        line.backgroundPrimaryColor, clampedOpacity);
  }

  private isTranslatedLineVertical(line: TranslatedLineData): boolean {
    const writingDirection =
        this.renderedTranslateParagraphs[line.paragraphIndex].writingDirection;
    return writingDirection === WritingDirection.kTopToBottom;
  }

  private getWritingModeForLine(line: TranslatedLineData): string {
    if (this.isTranslatedLineVertical(line)) {
      return 'vertical-lr';
    }
    return 'horizontal-tb';
  }

  private getLineAlignment(alignment: Alignment|null): string {
    if (alignment === Alignment.kDefaultLeftAlgined) {
      return 'left';
    } else if (alignment === Alignment.kCenterAligned) {
      return 'center';
    } else if (alignment === Alignment.kRightAligned) {
      return 'right';
    }

    return 'center';
  }

  private resetCanvasPixelRatioIfNeeded() {
    const transform = this.context.getTransform();
    if (transform.a !== window.devicePixelRatio ||
        transform.d !== window.devicePixelRatio) {
      this.context.setTransform(
          window.devicePixelRatio, 0, 0, window.devicePixelRatio, 0, 0);
    }
  }

  private getBlobUrlFromImageData(imageData: BackgroundImageData): string {
    const imageBytesBuffer = imageData.backgroundImage;
    assert(imageBytesBuffer.invalidBuffer !== true);
    let bytes: Uint8Array = new Uint8Array();
    if (imageBytesBuffer.bytes !== undefined) {
      bytes = new Uint8Array(imageBytesBuffer.bytes);
    } else if (imageBytesBuffer.sharedMemory !== undefined) {
      const {bufferHandle, size} = imageBytesBuffer.sharedMemory;
      const {buffer} = bufferHandle.mapBuffer(0, size);
      bytes = new Uint8Array(buffer);
    } else {
      return '';
    }
    // The image should always be a webp image.
    const blob = new Blob([bytes], {type: 'image/webp'});
    return URL.createObjectURL(blob);
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
    const elements = this.shadowRoot!.elementsFromPoint(x, y);
    if (elements.length === 0) {
      return null;
    }

    const words: Word[] = [];
    for (const element of elements) {
      if (!(element instanceof HTMLElement)) {
        continue;
      }
      const wordIndex = this.$.wordsContainer.indexForElement(element);
      if (wordIndex !== null) {
        words.push(this.renderedWords[wordIndex]);
      }
    }

    const imageBounds = this.selectionOverlayRect;
    const normalizedX = (x - imageBounds.left) / imageBounds.width;
    const normalizedY = (y - imageBounds.top) / imageBounds.height;
    const detectedWord = bestHit(words, {x: normalizedX, y: normalizedY});
    if (detectedWord === null) {
      return null;
    }

    const detectedWordIndex = this.renderedWords.indexOf(detectedWord);
    // `indexOf()` returns -1 when index not found.
    if (detectedWordIndex < 0) {
      return null;
    }
    return this.shouldRenderTranslateWords ?
        this.detectedWordToTranslateIndex[detectedWordIndex] :
        detectedWordIndex;
  }

  /**
   *
   * @returns Returns the index in renderedTranslateWords of the word at the
   *     given point. Returns null if no word is at the given point.
   */
  private translatedWordIndexFromPoint(x: number, y: number): number|null {
    if (!this.shouldRenderTranslateWords) {
      return null;
    }

    const topMostElement = this.shadowRoot!.elementFromPoint(x, y);
    if (!topMostElement || !(topMostElement instanceof HTMLElement)) {
      return null;
    }

    const wordIndexString = topMostElement.dataset['wordIndex'];
    if (!wordIndexString) {
      return null;
    }

    return parseInt(wordIndexString) ?? null;
  }

  private getTranslateLanguageDirection(translatedParagraph:
                                            TranslatedParagraph) {
    const language = translatedParagraph.contentLanguage ?
        translatedParagraph.contentLanguage :
        this.currentTranslateLanguage;
    if (!language) {
      return 'ltr';
    }
    return isRtlLanguage(language) ? 'rtl' : 'ltr';
  }

  // Testing method to get the words on the page.
  getWordNodesForTesting() {
    return this.shadowRoot!.querySelectorAll('.word');
  }

  // Testing method to get the translated words on the page.
  getTranslatedWordNodesForTesting() {
    return this.shadowRoot!.querySelectorAll('.translated-word');
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
