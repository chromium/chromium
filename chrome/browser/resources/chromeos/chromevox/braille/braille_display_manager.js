// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Puts text on a braille display.
 *
 */

goog.provide('BrailleDisplayManager');

goog.require('BrailleCaptionsBackground');
goog.require('BrailleDisplayState');
goog.require('ExpandingBrailleTranslator');
goog.require('LibLouis');
goog.require('NavBraille');
goog.require('PanStrategy');


/**
 * @param {!BrailleTranslatorManager} translatorManager Keeps track
 *     of the current translator to use.
 * @constructor
 */
BrailleDisplayManager = function(translatorManager) {
  /**
   * @type {!BrailleTranslatorManager}
   * @private
   */
  this.translatorManager_ = translatorManager;
  /**
   * @type {!NavBraille}
   * @private
   */
  this.content_ = new NavBraille({});
  /**
   * @type {!ExpandingBrailleTranslator.ExpansionType} valueExpansion
   * @private
   */
  this.expansionType_ = ExpandingBrailleTranslator.ExpansionType.SELECTION;
  /**
   * @type {PanStrategy}
   * @private
   */
  this.panStrategy_ = new PanStrategy();
  /**
   * @type {function(!BrailleKeyEvent, !NavBraille)}
   * @private
   */
  this.commandListener_ = function() {};
  /**
   * Current display state to show in the Virtual Braille Captions display.
   * This is different from realDisplayState_ if the braille captions feature
   * is enabled and there is no hardware display connected.  Otherwise, it is
   * the same object as realDisplayState_.
   * @type {!BrailleDisplayState}
   * @private
   */
  this.displayState_ = {available: false, textRowCount: 0, textColumnCount: 0};
  /**
   * State reported from the chrome api, reflecting a real hardware
   * display.
   * @type {!BrailleDisplayState}
   * @private
   */
  this.realDisplayState_ = this.displayState_;

  translatorManager.addChangeListener(function() {
    this.translateContent_(this.content_, this.expansionType_);
  }.bind(this));

  chrome.storage.onChanged.addListener(function(changes, area) {
    if (area == 'local' && 'brailleWordWrap' in changes) {
      this.updatePanStrategy_(changes.brailleWordWrap.newValue);
    }
    if (area == 'local' &&
        ('virtualBrailleRows' in changes ||
         'virtualBrailleColumns' in changes)) {
      this.onCaptionsStateChanged_();
    }
  }.bind(this));
  chrome.storage.local.get({brailleWordWrap: true}, function(items) {
    this.updatePanStrategy_(items.brailleWordWrap);
  }.bind(this));

  BrailleCaptionsBackground.init(goog.bind(this.onCaptionsStateChanged_, this));
  if (goog.isDef(chrome.brailleDisplayPrivate)) {
    var onDisplayStateChanged = goog.bind(this.refreshDisplayState_, this);
    chrome.brailleDisplayPrivate.getDisplayState(onDisplayStateChanged);
    chrome.brailleDisplayPrivate.onDisplayStateChanged.addListener(
        onDisplayStateChanged);
    chrome.brailleDisplayPrivate.onKeyEvent.addListener(
        goog.bind(this.onKeyEvent_, this));
  } else {
    // Get the initial captions state since we won't refresh the display
    // state in an API callback in this case.
    this.onCaptionsStateChanged_();
  }
};


/**
 * Dots representing a cursor.
 * @const
 */
BrailleDisplayManager.CURSOR_DOTS = 1 << 6 | 1 << 7;


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


/**
 * @param {!NavBraille} content Content to send to the braille display.
 * @param {!ExpandingBrailleTranslator.ExpansionType} expansionType
 *     If the text has a {@code ValueSpan}, this indicates how that part
 *     of the display content is expanded when translating to braille.
 *     (See {@code ExpandingBrailleTranslator}).
 */
BrailleDisplayManager.prototype.setContent = function(content, expansionType) {
  this.translateContent_(content, expansionType);
};


/**
 * Takes an image, in the form of a data url, and displays it in braille
 * onto the physical braille display and the virtual braille captions display.
 * @param {!string} imageUrl The image, in the form of a data url.
 */
BrailleDisplayManager.prototype.setImageContent = function(imageUrl) {
  if (!this.displayState_.available) {
    return;
  }

  // The number of dots in a braille cell.
  // TODO(dmazzoni): Both multi-line braille displays we're testing with
  // are 6-dot (2 x 3), but we should have a way to detect that via brltty.
  var cellWidth = 2;
  var cellHeight = 3;
  var maxCellHeight = 4;

  var rows = this.displayState_.textRowCount;
  var columns = this.displayState_.textColumnCount;
  var imageDataUrl = imageUrl;
  var imgElement = document.createElement('img');
  imgElement.src = imageDataUrl;
  imgElement.onload = function() {
    var canvas = document.createElement('canvas');
    var context = canvas.getContext('2d');
    canvas.width = columns * cellWidth;
    canvas.height = rows * cellHeight;
    context.drawImage(imgElement, 0, 0, canvas.width, canvas.height);
    var imageData = context.getImageData(0, 0, canvas.width, canvas.height);
    var data = imageData.data;
    var outputData = [];

    // Convert image to black and white by thresholding the luminance for
    // all opaque (non-transparent) pixels.
    for (var i = 0; i < data.length; i += 4) {
      var red = data[i];
      var green = data[i + 1];
      var blue = data[i + 2];
      var alpha = data[i + 3];
      var luminance = 0.2126 * red + 0.7152 * green + 0.0722 * blue;
      // Show braille pin if the alpha is greater than the threshold and
      // the luminance is less than the threshold.
      var show =
          (alpha >= BrailleDisplayManager.ALPHA_THRESHOLD_ &&
           luminance < BrailleDisplayManager.LUMINANCE_THRESHOLD_);
      outputData.push(show);
    }

    // Convert black-and-white array to the proper encoding for Braille
    // cells.
    var brailleBuf = new ArrayBuffer(rows * columns);
    var view = new Uint8Array(brailleBuf);
    for (var i = 0; i < rows; i++) {
      for (var j = 0; j < columns; j++) {
        // Index in braille array
        var brailleIndex = i * columns + j;
        for (var cellColumn = 0; cellColumn < cellWidth; cellColumn++) {
          for (var cellRow = 0; cellRow < cellHeight; cellRow++) {
            var bitmapIndex =
                (i * columns * cellHeight + j + cellRow * columns) * cellWidth +
                cellColumn;
            if (outputData[bitmapIndex]) {
              var index = cellColumn * maxCellHeight + cellRow;
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
  }.bind(this);
};


/**
 * Sets the command listener.  When a command is invoked, the listener will be
 * called with the BrailleKeyEvent corresponding to the command and the content
 * that was present on the display when the command was invoked.  The content
 * is guaranteed to be identical to an object previously used as the parameter
 * to BrailleDisplayManager.setContent, or null if no content was set.
 * @param {function(!BrailleKeyEvent, !NavBraille)} func The listener.
 */
BrailleDisplayManager.prototype.setCommandListener = function(func) {
  this.commandListener_ = func;
};


/**
 * @return {!BrailleDisplayState} The current display state.
 */
BrailleDisplayManager.prototype.getDisplayState = function() {
  return this.displayState_;
};


/**
 * @param {{available: boolean, textRowCount: (number|undefined),
 *     textColumnCount: (number|undefined)}} newState Display state reported
 *     by the extension API. Note that the type is almost the same as
 *     BrailleDisplayState except that the extension API allows
 *     some fields to be undefined, while BrailleDisplayState does not.
 * @private
 */
BrailleDisplayManager.prototype.refreshDisplayState_ = function(newState) {
  var oldColumnCount = this.displayState_.textColumnCount || 0;
  var oldRowCount = this.displayState_.textRowCount || 0;
  var processDisplayState = function(displayState) {
    this.displayState_ = displayState;
    var newColumnCount = displayState.textColumnCount || 0;
    var newRowCount = displayState.textRowCount || 0;

    if (oldColumnCount != newColumnCount || oldRowCount != newRowCount) {
      this.panStrategy_.setDisplaySize(newRowCount, newColumnCount);
    }
    this.refresh_();
  }.bind(this);
  this.realDisplayState_ = {
    available: newState.available,
    textRowCount: newState.textRowCount || 0,
    textColumnCount: newState.textColumnCount || 0
  };
  if (newState.available) {
    processDisplayState(newState);
    // Update the dimensions of the virtual braille captions display to those
    // of a real physical display when one is plugged in.
  } else {
    BrailleCaptionsBackground.getVirtualDisplayState(processDisplayState);
  }
};


/**
 * Called when the state of braille captions changes.
 * @private
 */
BrailleDisplayManager.prototype.onCaptionsStateChanged_ = function() {
  // Force reevaluation of the display state based on our stored real
  // hardware display state, meaning that if a real display is connected,
  // that takes precedence over the state from the captions 'virtual' display.
  this.refreshDisplayState_(this.realDisplayState_);
};


/**
 * Refreshes what is shown on the physical braille display and the virtual
 * braille captions display.
 * @private
 */
BrailleDisplayManager.prototype.refresh_ = function() {
  if (this.blinkerId_ !== undefined) {
    window.clearInterval(this.blinkerId_);
  }

  // If there's no cursor, don't schedule blinking.
  var cursor = this.panStrategy_.getCursor();
  var hideCursor = cursor.start == -1 || cursor.end == -1;

  this.refreshInternal_(!hideCursor);
  if (hideCursor) {
    return;
  }

  var showCursor = false;
  this.blinkerId_ = window.setInterval(function() {
    this.refreshInternal_(showCursor);
    showCursor = !showCursor;
  }.bind(this), BrailleDisplayManager.CURSOR_BLINK_TIME_MS);
};

/**
 * @param {boolean} showCursor Whether to show the cursor.
 * @private
 */
BrailleDisplayManager.prototype.refreshInternal_ = function(showCursor) {
  if (!this.displayState_.available) {
    return;
  }
  var brailleBuf =
      this.panStrategy_.getCurrentBrailleViewportContents(showCursor);
  var textBuf = this.panStrategy_.getCurrentTextViewportContents();
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
};

/**
 * @param {!NavBraille} newContent New display content.
 * @param {ExpandingBrailleTranslator.ExpansionType} newExpansionType
 *     How the value part of of the new content should be expanded
 *     with regards to contractions.
 * @private
 */
BrailleDisplayManager.prototype.translateContent_ = function(
    newContent, newExpansionType) {
  var writeTranslatedContent = function(cells, textToBraille, brailleToText) {
    this.content_ = newContent;
    this.expansionType_ = newExpansionType;
    var startIndex = this.content_.startIndex;
    var endIndex = this.content_.endIndex;
    var targetPosition;
    if (startIndex >= 0) {
      var translatedStartIndex;
      var translatedEndIndex;
      if (startIndex >= textToBraille.length) {
        // Allow the cells to be extended with one extra cell for
        // a carret after the last character.
        var extCells = new ArrayBuffer(cells.byteLength + 1);
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
  }.bind(this);

  var translator = this.translatorManager_.getExpandingTranslator();
  if (!translator) {
    writeTranslatedContent(new ArrayBuffer(0), [], []);
  } else {
    translator.translate(
        newContent.text, newExpansionType, writeTranslatedContent);
  }
};


/**
 * @param {BrailleKeyEvent} event The key event.
 * @private
 */
BrailleDisplayManager.prototype.onKeyEvent_ = function(event) {
  switch (event.command) {
    case BrailleKeyCommand.PAN_LEFT:
      this.panLeft_();
      break;
    case BrailleKeyCommand.PAN_RIGHT:
      this.panRight_();
      break;
    case BrailleKeyCommand.ROUTING:
      event.displayPosition = this.brailleToTextPosition_(
          event.displayPosition +
          this.panStrategy_.viewPort.firstRow *
              this.panStrategy_.displaySize.columns);
    // fall through
    default:
      this.commandListener_(event, this.content_);
      break;
  }
};


/**
 * Shift the display by one full display size and refresh the content.
 * Sends the appropriate command if the display is already at the leftmost
 * position.
 * @private
 */
BrailleDisplayManager.prototype.panLeft_ = function() {
  if (this.panStrategy_.previous()) {
    this.refresh_();
  } else {
    this.commandListener_({command: BrailleKeyCommand.PAN_LEFT}, this.content_);
  }
};


/**
 * Shifts the display position to the right by one full display size and
 * refreshes the content.  Sends the appropriate command if the display is
 * already at its rightmost position.
 * @private
 */
BrailleDisplayManager.prototype.panRight_ = function() {
  if (this.panStrategy_.next()) {
    this.refresh_();
  } else {
    this.commandListener_(
        {command: BrailleKeyCommand.PAN_RIGHT}, this.content_);
  }
};

/**
 * Sets a cursor within translated content.
 * @param {ArrayBuffer} buffer Buffer to add cursor to.
 * @param {number} startIndex The start index to place the cursor.
 * @param {number} endIndex The end index to place the cursor (exclusive).
 * @private
 */
BrailleDisplayManager.prototype.setCursor_ = function(
    buffer, startIndex, endIndex) {
  if (startIndex < 0 || startIndex >= buffer.byteLength ||
      endIndex < startIndex || endIndex > buffer.byteLength) {
    this.panStrategy_.setCursor(-1, -1);
    return;
  }
  if (startIndex == endIndex) {
    endIndex = startIndex + 1;
  }
  this.panStrategy_.setCursor(startIndex, endIndex);
};

/**
 * Returns the text position corresponding to an absolute braille position,
 * that is not accounting for the current pan position.
 * @private
 * @param {number} braillePosition Braille position relative to the startof
 *        the translated content.
 * @return {number} The mapped position in code units.
 */
BrailleDisplayManager.prototype.brailleToTextPosition_ = function(
    braillePosition) {
  var mapping = this.panStrategy_.brailleToText;
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
};

/**
 * @param {boolean} wordWrap
 * @private
 */
BrailleDisplayManager.prototype.updatePanStrategy_ = function(wordWrap) {
  this.panStrategy_.setPanStrategy(wordWrap);
  this.refresh_();
};
