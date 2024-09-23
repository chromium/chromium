// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Puts text on a braille display.
 */
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {BrailleDisplayState, BrailleKeyCommand, BrailleKeyEvent} from '../../common/braille/braille_key_types.js';
import {NavBraille} from '../../common/braille/nav_braille.js';
import {SettingsManager} from '../../common/settings_manager.js';

import {BrailleCaptionsBackground, BrailleCaptionsListener} from './braille_captions_background.js';
import {BrailleTranslatorManager} from './braille_translator_manager.js';
import {ExpandingBrailleTranslator} from './expanding_braille_translator.js';
import {PanStrategy} from './pan_strategy.js';

interface StateWithMaxCellHeight {
  rows: number;
  columns: number;
  cellWidth: number;
  cellHeight: number;
  maxCellHeight: number;
}

type CommandListener = (event: BrailleKeyEvent, content: NavBraille) => void;

export class BrailleDisplayManager implements BrailleCaptionsListener {
  private blinkerId_?: number;
  private content_ = new NavBraille({});
  private commandListener_: CommandListener = () => {};
  /**
   * Current display state to show in the Virtual Braille Captions display.
   * This is different from realDisplayState_ if the braille captions feature
   * is enabled and there is no hardware display connected.  Otherwise, it is
   * the same object as realDisplayState_.
   */
  private displayState_: BrailleDisplayState =
      {available: false, textRowCount: 0, textColumnCount: 0, cellSize: 0};
  private expansionType_ = ExpandingBrailleTranslator.ExpansionType.SELECTION;
  private panStrategy_ = new PanStrategy();
  /**
   * State reported from the chrome api, reflecting a real hardware
   * display.
   */
  private realDisplayState_: BrailleDisplayState = this.displayState_;

  static instance: BrailleDisplayManager;

  constructor() {
    BrailleTranslatorManager.instance.addChangeListener(
        () => this.translateContent_(this.content_, this.expansionType_));

    SettingsManager.addListenerForKey(
        'brailleWordWrap', (wrap: boolean) => this.updatePanStrategy_(wrap));
    SettingsManager.addListenerForKey(
        'virtualBrailleRows', () => this.onBrailleCaptionsStateChanged());
    SettingsManager.addListenerForKey(
        'virtualBrailleColumns', () => this.onBrailleCaptionsStateChanged());

    this.updatePanStrategy_(SettingsManager.getBoolean('brailleWordWrap'));

    BrailleCaptionsBackground.init(this);
    if (chrome.brailleDisplayPrivate !== undefined) {
      const onDisplayStateChanged =
          (newState: chrome.brailleDisplayPrivate.DisplayState): void =>
              this.refreshDisplayState_(newState);
      chrome.brailleDisplayPrivate.getDisplayState(onDisplayStateChanged);
      chrome.brailleDisplayPrivate.onDisplayStateChanged.addListener(
          onDisplayStateChanged);
      chrome.brailleDisplayPrivate.onKeyEvent.addListener(
          (event: BrailleKeyEvent) => this.onKeyEvent_(event));
    } else {
      // Get the initial captions state since we won't refresh the display
      // state in an API callback in this case.
      this.onBrailleCaptionsStateChanged();
    }
  }

  static init(): void {
    if (BrailleDisplayManager.instance) {
      throw new Error('Cannot create two BrailleDisplayManager instances');
    }
    BrailleDisplayManager.instance = new BrailleDisplayManager();
  }

  /**
   * Takes an image, in the form of a data url, and converts it to braille.
   * @param imageUrl The image, in the form of a data url.
   * @return The image, encoded in binary form, suitable for writing to a
   *     braille display.
   */
  static async convertImageDataUrlToBraille(
      imageUrl: string, displayState: BrailleDisplayState)
      : Promise<ArrayBuffer> {
    // The number of dots in a braille cell.
    // All known displays have a cell width of 2.
    const cellWidth = 2;
    // Derive the cell height from the display's cell size and the width above
    // e.g. 8 / 2 = 4.
    let cellHeight = displayState.cellSize / cellWidth;

    // Sanity check.
    if (cellHeight === 0) {
      // Set the height to a reasonable min.
      cellHeight = 3;
    }

    // All known displays don't exceed a cell height of 4.
    const maxCellHeight = 4;

    const rows = displayState.textRowCount;
    const columns = displayState.textColumnCount;
    const imageDataUrl = imageUrl;

    return new Promise<ArrayBuffer>((resolve: (buf: ArrayBuffer) => void) => {
      const imgElement = document.createElement('img');
      imgElement.src = imageDataUrl;
      imgElement.onload = () => {
        const canvas = document.createElement('canvas');
        // TODO(b/314203187): Not null asserted, check that this is correct.
        const context = canvas.getContext('2d')!;
        canvas.width = columns * cellWidth;
        canvas.height = rows * cellHeight;
        context.drawImage(imgElement, 0, 0, canvas.width, canvas.height);
        const imageData =
            context.getImageData(0, 0, canvas.width, canvas.height);
        const brailleBuf = BrailleDisplayManager.convertImageDataToBraille(
            imageData.data,
            {rows, columns, cellWidth, cellHeight, maxCellHeight});
        resolve(brailleBuf);
      };
    });
  }

  /**
   * @param data Encodes pixel data p_1, ..., p_n in groupings of RGBA. For
   *     example, for pixel 1, p_1_r, p_1_g, p_1_b, p_1_a. 1 ... n go from left
   *     to right, top to bottom.
   *
   *     The array looks like:
   *     [p_1_r, p_1_g, p_1_b, p_1_a, ... p_n_r, p_n_g, p_n_b, p_n_a].
   * @param state Dimensions of the braille display in cell units (number of
   *     rows, columns) and dot units (cell*).
   * @return a buffer encoding the braille dots according to that expected by
   *     BRLTTY.
   */
  static convertImageDataToBraille(
      data: Uint8ClampedArray, state: StateWithMaxCellHeight): ArrayBuffer {
    const {rows, columns, cellWidth, cellHeight, maxCellHeight} = state;
    const outputData = [];

    // The data should have groupings of 4 i.e. visible by 4.
    if (data.length % 4 !== 0) {
      return new ArrayBuffer(0);
    }

    // Convert image to black and white by thresholding the luminance for
    // all opaque (non-transparent) pixels.
    for (let i = 0; i < data.length; i += 4) {
      const red = data[i];
      const green = data[i + 1];
      const blue = data[i + 2];
      const alpha = data[i + 3];
      const luminance = 0.2126 * red + 0.7152 * green + 0.0722 * blue;

      // TODO(accessibility): this is a naive way to threshold. Consider
      // computing a global threshold based on an average of the top two most
      // frequent values using a histogram.

      // Show braille pin if the alpha is greater than the threshold and
      // the luminance is less than the threshold.
      const show =
          (alpha >= ALPHA_THRESHOLD &&
           luminance < LUMINANCE_THRESHOLD);
      outputData.push(show);
    }

    // Pick the mapping for the given cell height (default to 6-dot).
    const DOT_MAP =
        cellHeight === 4 ? COORDS_TO_BRAILLE_8DOT : COORDS_TO_BRAILLE_6DOT;

    // Convert black-and-white array to the proper encoding for Braille
    // cells.
    const brailleBuf = new ArrayBuffer(rows * columns);
    const view = new Uint8Array(brailleBuf);
    for (let i = 0; i < rows; i++) {
      for (let j = 0; j < columns; j++) {
        // Index in braille array.
        const brailleIndex = i * columns + j;
        for (let cellColumn = 0; cellColumn < cellWidth; cellColumn++) {
          for (let cellRow = 0; cellRow < cellHeight; cellRow++) {
            const bitmapIndex =
                (i * columns * cellHeight + j + cellRow * columns) * cellWidth +
                cellColumn;

            if (outputData[bitmapIndex]) {
              const index = cellColumn * maxCellHeight + cellRow;
              view[brailleIndex] += DOT_MAP[index];
            }
          }
        }
      }
    }
    return brailleBuf;
  }

  /**
   * @param content Content to send to the braille display.
   * @param expansionType If the text has a {@code ValueSpan}, this indicates
   *     how that part of the display content is expanded when translating to
   *     braille. (See {@code ExpandingBrailleTranslator}).
   */
  setContent(
      content: NavBraille,
      expansionType: ExpandingBrailleTranslator.ExpansionType): void {
    this.translateContent_(content, expansionType);
  }

  /**
   * Takes an image, in the form of a data url, and displays it in braille
   * onto the physical braille display and the virtual braille captions display.
   * @param imageUrl The image, in the form of a data url.
   */
  setImageContent(imageUrl: string): void {
    if (!this.displayState_.available) {
      return;
    }

    BrailleDisplayManager
        .convertImageDataUrlToBraille(imageUrl, this.displayState_)
        .then((brailleBuf: ArrayBuffer) => {
          if (this.realDisplayState_.available) {
            chrome.brailleDisplayPrivate.writeDots(
                brailleBuf, this.displayState_.textColumnCount,
                this.displayState_.textRowCount);
          }
          if (BrailleCaptionsBackground.isEnabled()) {
            BrailleCaptionsBackground.setImageContent(
                brailleBuf, this.displayState_.textRowCount,
                this.displayState_.textColumnCount);
          }
        });
  }

  /**
   * Sets the command listener.  When a command is invoked, the listener will be
   * called with the BrailleKeyEvent corresponding to the command and the
   * content that was present on the display when the command was invoked.  The
   * content is guaranteed to be identical to an object previously used as the
   * parameter to BrailleDisplayManager.setContent, or null if no content was
   * set.
   */
  setCommandListener(func: CommandListener): void {
    this.commandListener_ = func;
  }

  /** @return The current display state. */
  getDisplayState(): BrailleDisplayState {
    return this.displayState_;
  }

  /**
   * @param newState Display state reported by the extension API. Note that the
   *     type is almost the same as BrailleDisplayState except that the
   *     extension API allows some fields to be undefined, while
   *     BrailleDisplayState does not.
   */
  private refreshDisplayState_(
      newState: chrome.brailleDisplayPrivate.DisplayState): void {
    const oldColumnCount = this.displayState_.textColumnCount || 0;
    const oldRowCount = this.displayState_.textRowCount || 0;
    const oldCellSize = this.displayState_.cellSize || 0;
    const processDisplayState = (displayState: BrailleDisplayState): void => {
      this.displayState_ = displayState;
      const newColumnCount = displayState.textColumnCount || 0;
      const newRowCount = displayState.textRowCount || 0;
      const newCellSize = displayState.cellSize || 0;

      if (oldColumnCount !== newColumnCount || oldRowCount !== newRowCount ||
          oldCellSize !== newCellSize) {
        // TODO(accessibility): Audit whether changes in cellSize need to be
        // communicated to PanStrategy.
        this.panStrategy_.setDisplaySize(newRowCount, newColumnCount);
      }
      this.refresh_();
    };
    this.realDisplayState_ = {
      available: newState.available,
      textRowCount: newState.textRowCount || 0,
      textColumnCount: newState.textColumnCount || 0,
      cellSize: newState.cellSize || 0,
    };
    if (newState.available) {
      // Update the dimensions of the virtual braille captions display to those
      // of a real physical display when one is plugged in.
      processDisplayState(newState as BrailleDisplayState);
      SettingsManager.set('menuBrailleCommands', true);
    } else {
      processDisplayState(BrailleCaptionsBackground.getVirtualDisplayState());
      SettingsManager.set('menuBrailleCommands', false);
    }
  }

  /**
   * BrailleCaptionsListener implementation.
   * Called when the state of braille captions changes.
   */
  onBrailleCaptionsStateChanged(): void {
    // Force reevaluation of the display state based on our stored real
    // hardware display state, meaning that if a real display is connected,
    // that takes precedence over the state from the captions 'virtual' display.
    this.refreshDisplayState_(this.realDisplayState_);
  }

  /**
   * Refreshes what is shown on the physical braille display and the virtual
   * braille captions display.
   */
  private refresh_(): void {
    if (this.blinkerId_ !== undefined) {
      clearInterval(this.blinkerId_);
    }

    // If there's no cursor, don't schedule blinking.
    const cursor = this.panStrategy_.getCursor();
    const hideCursor = cursor.start === -1 || cursor.end === -1;

    this.refreshInternal_(!hideCursor);
    if (hideCursor) {
      return;
    }

    let showCursor = false;
    this.blinkerId_ = setInterval(() => {
      this.refreshInternal_(showCursor);
      showCursor = !showCursor;
    }, BrailleDisplayManager.CURSOR_BLINK_TIME_MS);
  }

  /** @param showCursor Whether to show the cursor. */
  private refreshInternal_(showCursor: boolean): void {
    if (!this.displayState_.available) {
      return;
    }
    const brailleBuf =
        this.panStrategy_.getCurrentBrailleViewportContents(showCursor);
    const textBuf = this.panStrategy_.getCurrentTextViewportContents();
    if (this.realDisplayState_.available) {
      chrome.brailleDisplayPrivate.writeDots(
          brailleBuf, this.realDisplayState_.textColumnCount,
          this.realDisplayState_.textRowCount);
    }
    if (BrailleCaptionsBackground.isEnabled()) {
      BrailleCaptionsBackground.setContent(
          textBuf, brailleBuf, this.panStrategy_.brailleToText,
          this.panStrategy_.offsetsForSlices, this.displayState_.textRowCount,
          this.displayState_.textColumnCount);
    }
  }

  /**
   * @param newContent New display content.
   * @param newExpansionType How the value part of of the new content should be
   *     expanded with regards to contractions.
   */
  private translateContent_(
      newContent: NavBraille,
      newExpansionType: ExpandingBrailleTranslator.ExpansionType): void {
    const writeTranslatedContent = (
        cells: ArrayBuffer, textToBraille: number[], brailleToText: number[])
        : void => {
      this.content_ = newContent;
      this.expansionType_ = newExpansionType;
      const startIndex = this.content_.startIndex;
      const endIndex = this.content_.endIndex;
      let targetPosition;
      if (startIndex >= 0) {
        let translatedStartIndex;
        let translatedEndIndex;
        if (startIndex >= textToBraille.length) {
          // Allow the cells to be extended with one extra cell for
          // a carret after the last character.
          const extCells = new ArrayBuffer(cells.byteLength + 1);
          new Uint8Array(extCells).set(new Uint8Array(cells));
          // Last byte is initialized to 0.
          cells = extCells;
          translatedStartIndex = cells.byteLength - 1;
        } else {
          translatedStartIndex = textToBraille[startIndex];
        }
        if (endIndex >= textToBraille.length) {
          // endIndex can't be past-the-end of the last cell unless
          // startIndex is too, so we don't have to do another
          // extension here.
          translatedEndIndex = cells.byteLength;
        } else {
          translatedEndIndex = textToBraille[endIndex];
        }
        // Add the cursor to cells.
        this.setCursor_(cells, translatedStartIndex, translatedEndIndex);
        targetPosition = translatedStartIndex;
      } else {
        this.setCursor_(cells, -1, -1);
        targetPosition = 0;
      }
      this.panStrategy_.setContent(
          this.content_.text.toString(), cells, brailleToText, targetPosition);

      this.refresh_();
    };

    const translator =
        BrailleTranslatorManager.instance.getExpandingTranslator();
    if (!translator) {
      writeTranslatedContent(new ArrayBuffer(0), [], []);
    } else {
      translator.translate(
          newContent.text, newExpansionType, writeTranslatedContent);
    }
  }

  private onKeyEvent_(event: BrailleKeyEvent): void {
    switch (event.command) {
      case BrailleKeyCommand.PAN_LEFT:
        this.panLeft();
        break;
      case BrailleKeyCommand.PAN_RIGHT:
        this.panRight();
        break;
      case BrailleKeyCommand.ROUTING:
        this.route(event.displayPosition);
        break;
      default:
        this.commandListener_(event, this.content_);
        break;
    }
  }

  /**
   * Shift the display by one full display size and refresh the content.
   * Sends the appropriate command if the display is already at the leftmost
   * position.
   */
  panLeft(): void {
    if (this.panStrategy_.previous()) {
      this.refresh_();
    } else {
      this.commandListener_(
          {command: BrailleKeyCommand.PAN_LEFT}, this.content_);
    }
  }

  /**
   * Shifts the display position to the right by one full display size and
   * refreshes the content.  Sends the appropriate command if the display is
   * already at its rightmost position.
   */
  panRight(): void {
    if (this.panStrategy_.next()) {
      this.refresh_();
    } else {
      this.commandListener_(
          {command: BrailleKeyCommand.PAN_RIGHT}, this.content_);
    }
  }

  /**
   * Moves the cursor to the given braille position.
   * @param braillePosition The 0-based position relative to the start of the
   *     currently displayed text. The position is given in braille cells, not
   *     text cells.
   */
  route(braillePosition?: number): void {
    if (braillePosition === undefined) {
      return;
    }
    const displayPosition = this.brailleToTextPosition_(
        braillePosition +
        this.panStrategy_.viewPort.firstRow *
            this.panStrategy_.displaySize.columns);
    this.commandListener_(
        {command: BrailleKeyCommand.ROUTING, displayPosition}, this.content_);
  }

  /**
   * Sets a cursor within translated content.
   * @param buffer Buffer to add cursor to.
   * @param startIndex The start index to place the cursor.
   * @param endIndex The end index to place the cursor (exclusive).
   */
  private setCursor_(
      buffer: ArrayBuffer, startIndex: number, endIndex: number): void {
    if (startIndex < 0 || startIndex >= buffer.byteLength ||
        endIndex < startIndex || endIndex > buffer.byteLength) {
      this.panStrategy_.setCursor(-1, -1);
      return;
    }
    if (startIndex === endIndex) {
      endIndex = startIndex + 1;
    }
    this.panStrategy_.setCursor(startIndex, endIndex);
  }

  /**
   * Returns the text position corresponding to an absolute braille position,
   * that is not accounting for the current pan position.
   * @param braillePosition Braille position relative to the startof
   *        the translated content.
   * @return The mapped position in code units.
   */
  private brailleToTextPosition_(braillePosition: number): number {
    const mapping = this.panStrategy_.brailleToText;
    if (braillePosition < 0) {
      // This shouldn't happen.
      console.error('WARNING: Braille position < 0: ' + braillePosition);
      return 0;
    } else if (braillePosition >= mapping.length) {
      // This happens when the user clicks on the right part of the display
      // when it is not entirely filled with content.  Allow addressing the
      // position after the last character.
      return this.content_.text.length;
    } else {
      return mapping[braillePosition];
    }
  }

  private updatePanStrategy_(wordWrap: boolean): void {
    this.panStrategy_.setPanStrategy(wordWrap);
    this.refresh_();
  }
}

export namespace BrailleDisplayManager {
  /**
   * Time elapsed before a cursor changes state. This results in a blinking
   * effect.
   */
  export const CURSOR_BLINK_TIME_MS = 1000;
}

// Local to module.

/**
 * Alpha threshold for a pixel to be possibly displayed as a raised dot when
 * converting an image to braille, where 255 means only fully-opaque
 * pixels can be raised (if their luminance passes the luminance threshold),
 * and 0 means that alpha is effectively ignored and only luminance matters.
 */
const ALPHA_THRESHOLD = 255;


/**
 * Luminance threshold for a pixel to be displayed as a raised dot when
 * converting an image to braille, on a scale of 0 (black) to 255 (white).
 * A pixel whose luminance is less than the given threshold will be raised.
 */
const LUMINANCE_THRESHOLD = 128;

/**
 * Array mapping an index in a 6-dot braille cell, in column-first order,
 * to its corresponding bit mask in the standard braille cell encoding.
 */
const COORDS_TO_BRAILLE_6DOT =
    [0x1, 0x2, 0x4, 0x8, 0x10, 0x20];


/**
 * Array mapping an index in an 8-dot braille cell, in column-first order,
 * to its corresponding bit mask in the standard braille cell encoding.
 */
const COORDS_TO_BRAILLE_8DOT =
    [0x1, 0x2, 0x4, 0x40, 0x8, 0x10, 0x20, 0x80];

TestImportManager.exportForTesting(BrailleDisplayManager);
