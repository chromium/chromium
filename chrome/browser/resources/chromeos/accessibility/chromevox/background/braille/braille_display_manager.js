// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Puts text on a braille display.
 */
import {LocalStorage} from '../../../common/local_storage.js';
import {BrailleDisplayState, BrailleKeyCommand, BrailleKeyEvent} from '../../common/braille/braille_key_types.js';
import {NavBraille} from '../../common/braille/nav_braille.js';
import {SettingsManager} from '../../common/settings_manager.js';

import {BrailleCaptionsBackground} from './braille_captions_background.js';
import {BrailleTranslatorManager} from './braille_translator_manager.js';
import {ExpandingBrailleTranslator} from './expanding_braille_translator.js';
import {PanStrategy} from './pan_strategy.js';
import {ValueSpan} from './spans.js';

export class BrailleDisplayManager {
  constructor() {
    /** @private {number|undefined} */
    this.blinkerId_;

    /** @private {!NavBraille} */
    this.content_ = new NavBraille({});

    /** @private {function(!BrailleKeyEvent, !NavBraille)} */
    this.commandListener_ = function() {};

    /**
     * Current display state to show in the Virtual Braille Captions display.
     * This is different from realDisplayState_ if the braille captions feature
     * is enabled and there is no hardware display connected.  Otherwise, it is
     * the same object as realDisplayState_.
     * @private {!BrailleDisplayState}
     */
    this.displayState_ = {
      available: false,
      textRowCount: 0,
      textColumnCount: 0,
    };

    /** @private {!ExpandingBrailleTranslator.ExpansionType} valueExpansion */
    this.expansionType_ = ExpandingBrailleTranslator.ExpansionType.SELECTION;

    /** @private {PanStrategy} */
    this.panStrategy_ = new PanStrategy();

    /**
     * State reported from the chrome api, reflecting a real hardware
     * display.
     * @private {!BrailleDisplayState}
     */
    this.realDisplayState_ = this.displayState_;

    this.init_();
  }

  /** @private */
  init_() {
    BrailleTranslatorManager.instance.addChangeListener(
        () => this.translateContent_(this.content_, this.expansionType_));

    SettingsManager.addListenerForKey(
        'brailleWordWrap', wrap => this.updatePanStrategy_(wrap));
    SettingsManager.addListenerForKey(
        'virtualBrailleRows', () => this.onCaptionsStateChanged_());
    SettingsManager.addListenerForKey(
        'virtualBrailleColumns', () => this.onCaptionsStateChanged_());

    this.updatePanStrategy_(SettingsManager.getBoolean('brailleWordWrap'));

    BrailleCaptionsBackground.init(() => this.onCaptionsStateChanged_());
    if (goog.isDef(chrome.brailleDisplayPrivate)) {
      const onDisplayStateChanged = newState =>
          this.refreshDisplayState_(newState);
      chrome.brailleDisplayPrivate.getDisplayState(onDisplayStateChanged);
      chrome.brailleDisplayPrivate.onDisplayStateChanged.addListener(
          onDisplayStateChanged);
      chrome.brailleDisplayPrivate.onKeyEvent.addListener(
          event => this.onKeyEvent_(event));
    } else {
      // Get the initial captions state since we won't refresh the display
      // state in an API callback in this case.
      this.onCaptionsStateChanged_();
    }
  }

  static init() {
    if (BrailleDisplayManager.instance) {
      throw new Error('Cannot create two BrailleDisplayManager instances');
    }
    BrailleDisplayManager.instance = new BrailleDisplayManager();
  }

  /**
   * @param {!NavBraille} content Content to send to the braille display.
   * @param {!ExpandingBrailleTranslator.ExpansionType} expansionType
   *     If the text has a {@code ValueSpan}, this indicates how that part
   *     of the display content is expanded when translating to braille.
   *     (See {@code ExpandingBrailleTranslator}).
   */
  setContent(content, expansionType) {
    this.translateContent_(content, expansionType);
  }

  /**
   * Takes an image, in the form of a data url, and displays it in braille
   * onto the physical braille display and the virtual braille captions display.
   * @param {!string} imageUrl The image, in the form of a data url.
   */
  setImageContent(imageUrl) {
    if (!this.displayState_.available) {
      return;
    }

    // The number of dots in a braille cell.
    // TODO: Both multi-line braille displays we're testing with
    // are 6-dot (2 x 3), but we should switch to detecting this with
    // BRLAPI_PARAM_DEVICE_CELL_SIZE from brlapi instead.
    const cellWidth = 2;
    const cellHeight = 3;
    const maxCellHeight = 4;

    const rows = this.displayState_.textRowCount;
    const columns = this.displayState_.textColumnCount;
    const imageDataUrl = imageUrl;
    const imgElement = document.createElement('img');
    imgElement.src = imageDataUrl;
    imgElement.onload = () => {
      const canvas = document.createElement('canvas');
      const context = canvas.getContext('2d');
      canvas.width = columns * cellWidth;
      canvas.height = rows * cellHeight;
      context.drawImage(imgElement, 0, 0, canvas.width, canvas.height);
      const imageData = context.getImageData(0, 0, canvas.width, canvas.height);
      const data = imageData.data;
      const outputData = [];

      // Convert image to black and white by thresholding the luminance for
      // all opaque (non-transparent) pixels.
      for (let i = 0; i < data.length; i += 4) {
        const red = data[i];
        const green = data[i + 1];
        const blue = data[i + 2];
        const alpha = data[i + 3];
        const luminance = 0.2126 * red + 0.7152 * green + 0.0722 * blue;
        // Show braille pin if the alpha is greater than the threshold and
        // the luminance is less than the threshold.
        const show =
            (alpha >= BrailleDisplayManager.ALPHA_THRESHOLD_ &&
             luminance < BrailleDisplayManager.LUMINANCE_THRESHOLD_);
        outputData.push(show);
      }

      // Convert black-and-white array to the proper encoding for Braille
      // cells.
      const brailleBuf = new ArrayBuffer(rows * columns);
      const view = new Uint8Array(brailleBuf);
      for (let i = 0; i < rows; i++) {
        for (let j = 0; j < columns; j++) {
          // Index in braille array
          const brailleIndex = i * columns + j;
          for (let cellColumn = 0; cellColumn < cellWidth; cellColumn++) {
            for (let cellRow = 0; cellRow < cellHeight; cellRow++) {
              const bitmapIndex =
                  (i * columns * cellHeight + j + cellRow * columns) *
                      cellWidth +
                  cellColumn;
              if (outputData[bitmapIndex]) {
                const index = cellColumn * maxCellHeight + cellRow;
                view[brailleIndex] +=
                    BrailleDisplayManager.COORDS_TO_BRAILLE_DOT_[index];
              }
            }
          }
        }
      }

      if (this.realDisplayState_.available) {
        chrome.brailleDisplayPrivate.writeDots(
            brailleBuf, this.displayState_.textColumnCount,
            this.displayState_.textRowCount);
      }
      if (BrailleCaptionsBackground.isEnabled()) {
        BrailleCaptionsBackground.setImageContent(brailleBuf, rows, columns);
      }
    };
  }

  /**
   * Sets the command listener.  When a command is invoked, the listener will be
   * called with the BrailleKeyEvent corresponding to the command and the
   * content that was present on the display when the command was invoked.  The
   * content is guaranteed to be identical to an object previously used as the
   * parameter to BrailleDisplayManager.setContent, or null if no content was
   * set.
   * @param {function(!BrailleKeyEvent, !NavBraille)} func The listener.
   */
  setCommandListener(func) {
    this.commandListener_ = func;
  }

  /**
   * @return {!BrailleDisplayState} The current display state.
   */
  getDisplayState() {
    return this.displayState_;
  }

  /**
   * @param {{available: boolean, textRowCount: (number|undefined),
   *     textColumnCount: (number|undefined)}} newState Display state reported
   *     by the extension API. Note that the type is almost the same as
   *     BrailleDisplayState except that the extension API allows
   *     some fields to be undefined, while BrailleDisplayState does not.
   * @private
   */
  refreshDisplayState_(newState) {
    const oldColumnCount = this.displayState_.textColumnCount || 0;
    const oldRowCount = this.displayState_.textRowCount || 0;
    const processDisplayState = displayState => {
      this.displayState_ = displayState;
      const newColumnCount = displayState.textColumnCount || 0;
      const newRowCount = displayState.textRowCount || 0;

      if (oldColumnCount !== newColumnCount || oldRowCount !== newRowCount) {
        this.panStrategy_.setDisplaySize(newRowCount, newColumnCount);
      }
      this.refresh_();
    };
    this.realDisplayState_ = {
      available: newState.available,
      textRowCount: newState.textRowCount || 0,
      textColumnCount: newState.textColumnCount || 0,
    };
    if (newState.available) {
      // Update the dimensions of the virtual braille captions display to those
      // of a real physical display when one is plugged in.
      processDisplayState(newState);
      SettingsManager.set('menuBrailleCommands', true);
    } else {
      processDisplayState(BrailleCaptionsBackground.getVirtualDisplayState());
      SettingsManager.set('menuBrailleCommands', false);
    }
  }

  /**
   * Called when the state of braille captions changes.
   * @private
   */
  onCaptionsStateChanged_() {
    // Force reevaluation of the display state based on our stored real
    // hardware display state, meaning that if a real display is connected,
    // that takes precedence over the state from the captions 'virtual' display.
    this.refreshDisplayState_(this.realDisplayState_);
  }

  /**
   * Refreshes what is shown on the physical braille display and the virtual
   * braille captions display.
   * @private
   */
  refresh_() {
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

  /**
   * @param {boolean} showCursor Whether to show the cursor.
   * @private
   */
  refreshInternal_(showCursor) {
    if (!this.displayState_.available) {
      return;
    }
    const brailleBuf =
        this.panStrategy_.getCurrentBrailleViewportContents(showCursor);
    const textBuf = this.panStrategy_.getCurrentTextViewportContents();
    if (this.realDisplayState_.available) {
      chrome.brailleDisplayPrivate.writeDots(
          brailleBuf, brailleBuf.byteLength, 1);
    }
    if (BrailleCaptionsBackground.isEnabled()) {
      BrailleCaptionsBackground.setContent(
          textBuf, brailleBuf, this.panStrategy_.brailleToText,
          this.panStrategy_.offsetsForSlices, this.displayState_.textRowCount,
          this.displayState_.textColumnCount);
    }
  }

  /**
   * @param {!NavBraille} newContent New display content.
   * @param {ExpandingBrailleTranslator.ExpansionType} newExpansionType
   *     How the value part of of the new content should be expanded
   *     with regards to contractions.
   * @private
   */
  translateContent_(newContent, newExpansionType) {
    const writeTranslatedContent = (cells, textToBraille, brailleToText) => {
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

  /**
   * @param {BrailleKeyEvent} event The key event.
   * @private
   */
  onKeyEvent_(event) {
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
  panLeft() {
    if (this.panStrategy_.previous()) {
      this.refresh_();
    } else {
      this.commandListener_(
          /** @type {!BrailleKeyEvent} */ (
              {command: BrailleKeyCommand.PAN_LEFT}),
          this.content_);
    }
  }

  /**
   * Shifts the display position to the right by one full display size and
   * refreshes the content.  Sends the appropriate command if the display is
   * already at its rightmost position.
   */
  panRight() {
    if (this.panStrategy_.next()) {
      this.refresh_();
    } else {
      this.commandListener_(
          /** @type {!BrailleKeyEvent} */ (
              {command: BrailleKeyCommand.PAN_RIGHT}),
          this.content_);
    }
  }

  /**
   * Moves the cursor to the given braille position.
   * @param {number|undefined} braillePosition The 0-based position relative to
   *     the start of the currently displayed text. The position is given in
   *     braille cells, not text cells.
   */
  route(braillePosition) {
    if (braillePosition === undefined) {
      return;
    }
    const displayPosition = this.brailleToTextPosition_(
        braillePosition +
        this.panStrategy_.viewPort.firstRow *
            this.panStrategy_.displaySize.columns);
    this.commandListener_(
        /** @type {!BrailleKeyEvent} */ (
            {command: BrailleKeyCommand.ROUTING, displayPosition}),
        this.content_);
  }

  /**
   * Sets a cursor within translated content.
   * @param {ArrayBuffer} buffer Buffer to add cursor to.
   * @param {number} startIndex The start index to place the cursor.
   * @param {number} endIndex The end index to place the cursor (exclusive).
   * @private
   */
  setCursor_(buffer, startIndex, endIndex) {
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
   * @private
   * @param {number} braillePosition Braille position relative to the startof
   *        the translated content.
   * @return {number} The mapped position in code units.
   */
  brailleToTextPosition_(braillePosition) {
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

  /**
   * @param {boolean} wordWrap
   * @private
   */
  updatePanStrategy_(wordWrap) {
    this.panStrategy_.setPanStrategy(wordWrap);
    this.refresh_();
  }
}

/**
 * Alpha threshold for a pixel to be possibly displayed as a raised dot when
 * converting an image to braille, where 255 means only fully-opaque
 * pixels can be raised (if their luminance passes the luminance threshold),
 * and 0 means that alpha is effectively ignored and only luminance matters.
 * @const
 * @private
 */
BrailleDisplayManager.ALPHA_THRESHOLD_ = 255;


/**
 * Luminance threshold for a pixel to be displayed as a raised dot when
 * converting an image to braille, on a scale of 0 (black) to 255 (white).
 * A pixel whose luminance is less than the given threshold will be raised.
 * @const
 * @private
 */
BrailleDisplayManager.LUMINANCE_THRESHOLD_ = 128;


/**
 * Array mapping an index in an 8-dot braille cell, in column-first order,
 * to its corresponding bit mask in the standard braille cell encoding.
 * @const
 * @private
 */
BrailleDisplayManager.COORDS_TO_BRAILLE_DOT_ =
    [0x1, 0x2, 0x4, 0x40, 0x8, 0x10, 0x20, 0x80];


/**
 * Time elapsed before a cursor changes state. This results in a blinking
 * effect.
 * @const {number}
 */
BrailleDisplayManager.CURSOR_BLINK_TIME_MS = 1000;

/** @type {BrailleDisplayManager} */
BrailleDisplayManager.instance;
