// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Turn a dictionary received from postMessage into a key event.
 *
 * @param {Object} dict A dictionary representing the key event.
 * @return {!Event} A key event.
 */
export function DeserializeKeyEvent(dict) {
  const e = document.createEvent('Event');
  e.initEvent('keydown', true, true);
  e.keyCode = dict.keyCode;
  e.code = dict.code;
  e.shiftKey = dict.shiftKey;
  e.ctrlKey = dict.ctrlKey;
  e.altKey = dict.altKey;
  e.metaKey = dict.metaKey;
  e.fromScriptingAPI = true;
  return e;
}

/**
 * Turn a key event into a dictionary which can be sent over postMessage.
 *
 * @param {Event} event A key event.
 * @return {Object} A dictionary representing the key event.
 */
export function SerializeKeyEvent(event) {
  return {
    keyCode: event.keyCode,
    code: event.code,
    shiftKey: event.shiftKey,
    ctrlKey: event.ctrlKey,
    altKey: event.altKey,
    metaKey: event.metaKey
  };
}

/**
 * An enum containing a value specifying whether the PDF is currently loading,
 * has finished loading or failed to load.
 * @enum {string}
 */
export const LoadState = {
  LOADING: 'loading',
  SUCCESS: 'success',
  FAILED: 'failed'
};

/**
 * Create a new PDFScriptingAPI. This provides a scripting interface to
 * the PDF viewer so that it can be customized by things like print preview.
 *
 */
export class PDFScriptingAPI {
  /**
   * @param {Window} window the window of the page containing the pdf viewer.
   * @param {Object} plugin the plugin element containing the pdf viewer.
   */
  constructor(window, plugin) {
    this.loadState_ = LoadState.LOADING;
    this.pendingScriptingMessages_ = [];
    this.setPlugin(plugin);

    /** @private {Function} */
    this.viewportChangedCallback_;

    /** @private {Function} */
    this.loadCallback_;

    /** @private {Function} */
    this.selectedTextCallback_;

    /** @private {Function} */
    this.keyEventCallback_;

    /** @private {Object} */
    this.plugin_;

    window.addEventListener('message', event => {
      if (event.origin !=
              'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai' &&
          event.origin != 'chrome://print') {
        console.error(
            'Received message that was not from the extension: ' + event);
        return;
      }
      switch (event.data.type) {
        case 'viewport':
          /**
           * @type {{
           *   pageX: number,
           *   pageY: number,
           *   pageWidth: number,
           *   viewportWidth: number,
           *   viewportHeight: number
           * }}
           */
          const viewportData = event.data;
          if (this.viewportChangedCallback_) {
            this.viewportChangedCallback_(
                viewportData.pageX, viewportData.pageY, viewportData.pageWidth,
                viewportData.viewportWidth, viewportData.viewportHeight);
          }
          break;
        case 'documentLoaded': {
          const data = /** @type {{load_state: LoadState}} */ (event.data);
          this.loadState_ = data.load_state;
          if (this.loadCallback_) {
            this.loadCallback_(this.loadState_ == LoadState.SUCCESS);
          }
          break;
        }
        case 'getSelectedTextReply': {
          const data = /** @type {{selectedText: string}} */ (event.data);
          if (this.selectedTextCallback_) {
            this.selectedTextCallback_(data.selectedText);
            this.selectedTextCallback_ = null;
          }
          break;
        }
        case 'sendKeyEvent':
          if (this.keyEventCallback_) {
            this.keyEventCallback_(DeserializeKeyEvent(event.data.keyEvent));
          }
          break;
      }
    }, false);
  }

  /**
   * Send a message to the extension. If messages try to get sent before there
   * is a plugin element set, then we queue them up and send them later (this
   * can happen in print preview).
   *
   * @param {Object} message The message to send.
   * @private
   */
  sendMessage_(message) {
    if (this.plugin_) {
      this.plugin_.postMessage(message, '*');
    } else {
      this.pendingScriptingMessages_.push(message);
    }
  }

  /**
   * Sets the plugin element containing the PDF viewer. The element will usually
   * be passed into the PDFScriptingAPI constructor but may also be set later.
   *
   * @param {Object} plugin the plugin element containing the PDF viewer.
   */
  setPlugin(plugin) {
    this.plugin_ = plugin;

    if (this.plugin_) {
      // Send a message to ensure the postMessage channel is initialized which
      // allows us to receive messages.
      this.sendMessage_({type: 'initialize'});
      // Flush pending messages.
      while (this.pendingScriptingMessages_.length > 0) {
        this.sendMessage_(this.pendingScriptingMessages_.shift());
      }
    }
  }

  /**
   * Sets the callback which will be run when the PDF viewport changes.
   *
   * @param {Function} callback the callback to be called.
   */
  setViewportChangedCallback(callback) {
    this.viewportChangedCallback_ = callback;
  }

  /**
   * Sets the callback which will be run when the PDF document has finished
   * loading. If the document is already loaded, it will be run immediately.
   *
   * @param {Function} callback the callback to be called.
   */
  setLoadCallback(callback) {
    this.loadCallback_ = callback;
    if (this.loadState_ != LoadState.LOADING && this.loadCallback_) {
      this.loadCallback_(this.loadState_ == LoadState.SUCCESS);
    }
  }

  /**
   * Sets a callback that gets run when a key event is fired in the PDF viewer.
   * @param {Function} callback the callback to be called with a key event.
   */
  setKeyEventCallback(callback) {
    this.keyEventCallback_ = callback;
  }

  /**
   * Resets the PDF viewer into print preview mode.
   *
   * @param {string} url the url of the PDF to load.
   * @param {boolean} grayscale whether or not to display the PDF in grayscale.
   * @param {Array<number>} pageNumbers an array of the page numbers.
   * @param {boolean} modifiable whether or not the document is modifiable.
   */
  resetPrintPreviewMode(url, grayscale, pageNumbers, modifiable) {
    this.loadState_ = LoadState.LOADING;
    this.sendMessage_({
      type: 'resetPrintPreviewMode',
      url: url,
      grayscale: grayscale,
      pageNumbers: pageNumbers,
      modifiable: modifiable
    });
  }

  /**
   * Hide the toolbars after a delay.
   */
  hideToolbars() {
    this.sendMessage_({type: 'hideToolbars'});
  }

  /**
   * Load a page into the document while in print preview mode.
   *
   * @param {string} url the url of the pdf page to load.
   * @param {number} index the index of the page to load.
   */
  loadPreviewPage(url, index) {
    this.sendMessage_({type: 'loadPreviewPage', url: url, index: index});
  }

  /** @param {boolean} darkMode Whether the page is in dark mode. */
  darkModeChanged(darkMode) {
    this.sendMessage_({type: 'darkModeChanged', darkMode: darkMode});
  }

  /**
   * Select all the text in the document. May only be called after document
   * load.
   */
  selectAll() {
    this.sendMessage_({type: 'selectAll'});
  }

  /**
   * Get the selected text in the document. The callback will be called with the
   * text that is selected. May only be called after document load.
   *
   * @param {Function} callback a callback to be called with the selected text.
   * @return {boolean} true if the function is successful, false if there is an
   *     outstanding request for selected text that has not been answered.
   */
  getSelectedText(callback) {
    if (this.selectedTextCallback_) {
      return false;
    }
    this.selectedTextCallback_ = callback;
    this.sendMessage_({type: 'getSelectedText'});
    return true;
  }

  /**
   * Print the document. May only be called after document load.
   */
  print() {
    this.sendMessage_({type: 'print'});
  }

  /**
   * Send a key event to the extension.
   *
   * @param {Event} keyEvent the key event to send to the extension.
   */
  sendKeyEvent(keyEvent) {
    this.sendMessage_(
        {type: 'sendKeyEvent', keyEvent: SerializeKeyEvent(keyEvent)});
  }

  /**
   * @param {number} scrollX The amount to horizontally scroll in pixels.
   * @param {number} scrollY The amount to vertically scroll in pixels.
   */
  scrollPosition(scrollX, scrollY) {
    this.sendMessage_({type: 'scrollPosition', x: scrollX, y: scrollY});
  }
}

/**
 * Creates a PDF viewer with a scripting interface. This is basically 1) an
 * iframe which is navigated to the PDF viewer extension and 2) a scripting
 * interface which provides access to various features of the viewer for use
 * by print preview and accessibility.
 *
 * @param {string} src the source URL of the PDF to load initially.
 * @param {string} baseUrl the base URL of the PDF viewer
 * @return {!HTMLIFrameElement} the iframe element containing the PDF viewer.
 */
export function PDFCreateOutOfProcessPlugin(src, baseUrl) {
  const client = new PDFScriptingAPI(window, null);
  const iframe = /** @type {!HTMLIFrameElement} */ (
      window.document.createElement('iframe'));
  iframe.setAttribute('src', baseUrl + '/index.html?' + src);

  iframe.onload = function() {
    client.setPlugin(iframe.contentWindow);
  };

  // Add the functions to the iframe so that they can be called directly.
  iframe.setViewportChangedCallback =
      client.setViewportChangedCallback.bind(client);
  iframe.setLoadCallback = client.setLoadCallback.bind(client);
  iframe.setKeyEventCallback = client.setKeyEventCallback.bind(client);
  iframe.resetPrintPreviewMode = client.resetPrintPreviewMode.bind(client);
  iframe.loadPreviewPage = client.loadPreviewPage.bind(client);
  iframe.sendKeyEvent = client.sendKeyEvent.bind(client);
  iframe.scrollPosition = client.scrollPosition.bind(client);
  iframe.hideToolbars = client.hideToolbars.bind(client);
  iframe.darkModeChanged = client.darkModeChanged.bind(client);
  return iframe;
}
