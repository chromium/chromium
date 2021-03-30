// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';
import {NativeEventTarget as EventTarget} from 'chrome://resources/js/cr/event_target.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';

import {NamedDestinationMessageData, Point, SaveRequestType} from './constants.js';
import {PartialPoint, PinchPhase, Viewport} from './viewport.js';

/** @typedef {{type: string, messageId: (string|undefined)}} */
export let MessageData;

/**
 * @typedef {{
 *   type: string,
 *   dataToSave: Array,
 *   messageId: string,
 * }}
 */
let SaveAttachmentDataMessageData;

/**
 * @typedef {{
 *   dataToSave: Array,
 *   token: string,
 *   fileName: string
 * }}
 */
let SaveDataMessageData;

/**
 * @typedef {{
 *   type: string,
 *   url: string,
 *   grayscale: boolean,
 *   modifiable: boolean,
 *   pageNumbers: !Array<number>
 * }}
 */
export let PrintPreviewParams;

/**
 * @typedef {{
 *   imageData: !ArrayBuffer,
 *   width: number,
 *   height: number,
 * }}
 */
let ThumbnailMessageData;

/**
 * Creates a cryptographically secure pseudorandom 128-bit token.
 * @return {string} The generated token as a hex string.
 */
function createToken() {
  const randomBytes = new Uint8Array(16);
  return window.crypto.getRandomValues(randomBytes)
      .map(b => b.toString(16).padStart(2, '0'))
      .join('');
}

/** @interface */
export class ContentController {
  constructor() {}

  /** @return {!EventTarget} */
  getEventTarget() {}

  /** @return {boolean} */
  get isActive() {}

  /** @param {boolean} isActive */
  set isActive(isActive) {}

  beforeZoom() {}

  afterZoom() {}

  viewportChanged() {}

  rotateClockwise() {}

  rotateCounterclockwise() {}

  /** @param {boolean} displayAnnotations */
  setDisplayAnnotations(displayAnnotations) {}

  /** @param {boolean} enableTwoUpView */
  setTwoUpView(enableTwoUpView) {}

  /** Triggers printing of the current document. */
  print() {}

  /** Undo an edit action. */
  undo() {}

  /** Redo an edit action. */
  redo() {}

  /**
   * Requests that the current document be saved.
   * @param {!SaveRequestType} requestType The type of save request. If
   *     ANNOTATION, a response is required, otherwise the controller may save
   *     the document to disk internally.
   * @return {!Promise<!{fileName: string, dataToSave: !ArrayBuffer}>}
   */
  save(requestType) {}

  /**
   * Requests that the attachment at a certain index be saved.
   * @param {number} index The index of the attachment to be saved.
   * @return {Promise<{type: string, dataToSave: Array, messageId: string}>}
   * @abstract
   */
  saveAttachment(index) {}

  /**
   * Loads PDF document from `data` activates UI.
   * @param {string} fileName
   * @param {!ArrayBuffer} data
   * @return {!Promise<void>}
   */
  load(fileName, data) {}

  /** Unloads the current document and removes the UI. */
  unload() {}
}

/**
 * Event types dispatched by the plugin controller.
 * @enum {string}
 */
export const PluginControllerEventType = {
  IS_ACTIVE_CHANGED: 'PluginControllerEventType.IS_ACTIVE_CHANGED',
  PLUGIN_MESSAGE: 'PluginControllerEventType.PLUGIN_MESSAGE',
};

/**
 * PDF plugin controller singleton, responsible for communicating with the
 * embedded plugin element. Dispatches a
 * `PluginControllerEventType.PLUGIN_MESSAGE` event containing the message from
 * the plugin, if a message type not handled by this controller is received.
 * @implements {ContentController}
 */
export class PluginController {
  constructor() {
    /** @private {!EventTarget} */
    this.eventTarget_ = new EventTarget();

    /** @private {boolean} */
    this.isActive_ = false;

    /** @private {!HTMLEmbedElement} */
    this.plugin_;

    /** @private {!Viewport} */
    this.viewport_;

    /** @private {!function():boolean} */
    this.getIsUserInitiatedCallback_;

    /** @private {!function():?Promise} */
    this.getLoadedCallback_;

    /** @private {!Map<string, PromiseResolver>} */
    this.pendingTokens_;

    /** @private {!Map<string, !PromiseResolver>} */
    this.requestResolverMap_;

    /**
     * Counter for use with createUid
     * @private {number}
     */
    this.uidCounter_ = 1;
  }

  /**
   * @param {!HTMLEmbedElement} plugin
   * @param {!Viewport} viewport
   * @param {function():boolean} getIsUserInitiatedCallback
   * @param {function():?Promise} getLoadedCallback
   */
  init(plugin, viewport, getIsUserInitiatedCallback, getLoadedCallback) {
    this.plugin_ = plugin;
    this.viewport_ = viewport;
    this.getIsUserInitiatedCallback_ = getIsUserInitiatedCallback;
    this.getLoadedCallback_ = getLoadedCallback;
    this.pendingTokens_ = new Map();
    this.requestResolverMap_ = new Map();

    this.plugin_.addEventListener(
        'message', e => this.handlePluginMessage_(e), false);
  }

  /**
   * @return {boolean}
   * @override
   */
  get isActive() {
    // Check whether `plugin_` is defined as a signal that `init()` was called.
    return !!this.plugin_ && this.isActive_;
  }

  /**
   * @param {boolean} isActive
   * @override
   */
  set isActive(isActive) {
    const wasActive = this.isActive;
    this.isActive_ = isActive;
    if (this.isActive === wasActive) {
      return;
    }

    this.eventTarget_.dispatchEvent(new CustomEvent(
        PluginControllerEventType.IS_ACTIVE_CHANGED, {detail: this.isActive}));
  }

  /**
   * @return {number} A new unique ID.
   * @private
   */
  createUid_() {
    return this.uidCounter_++;
  }

  /**
   * @return {!EventTarget}
   * @override
   */
  getEventTarget() {
    return this.eventTarget_;
  }

  /**
   * @param {number} x
   * @param {number} y
   */
  updateScroll(x, y) {
    this.postMessage_({type: 'updateScroll', x, y});
  }

  viewportChanged() {}

  redo() {}

  undo() {}

  /**
   * Notify the plugin to stop reacting to scroll events while zoom is taking
   * place to avoid flickering.
   * @override
   */
  beforeZoom() {
    this.postMessage_({type: 'stopScrolling'});

    if (this.viewport_.pinchPhase === PinchPhase.START) {
      const position = this.viewport_.position;
      const zoom = this.viewport_.getZoom();
      const pinchPhase = this.viewport_.pinchPhase;
      const layoutOptions = this.viewport_.getLayoutOptions();
      this.postMessage_({
        type: 'viewport',
        userInitiated: true,
        zoom: zoom,
        layoutOptions: layoutOptions,
        xOffset: position.x,
        yOffset: position.y,
        pinchPhase: pinchPhase
      });
    }
  }

  /**
   * Notify the plugin of the zoom change and to continue reacting to scroll
   * events.
   * @override
   */
  afterZoom() {
    const position = this.viewport_.position;
    const zoom = this.viewport_.getZoom();
    const layoutOptions = this.viewport_.getLayoutOptions();
    const pinchVector = this.viewport_.pinchPanVector || {x: 0, y: 0};
    const pinchCenter = this.viewport_.pinchCenter || {x: 0, y: 0};
    const pinchPhase = this.viewport_.pinchPhase;

    this.postMessage_({
      type: 'viewport',
      userInitiated: this.getIsUserInitiatedCallback_(),
      zoom: zoom,
      layoutOptions: layoutOptions,
      xOffset: position.x,
      yOffset: position.y,
      pinchPhase: pinchPhase,
      pinchX: pinchCenter.x,
      pinchY: pinchCenter.y,
      pinchVectorX: pinchVector.x,
      pinchVectorY: pinchVector.y
    });
  }

  /**
   * Post a message to the PPAPI plugin. Some messages will cause an async reply
   * to be received through handlePluginMessage_().
   * @param {!MessageData} message Message to post.
   * @private
   */
  postMessage_(message) {
    this.plugin_.postMessage(message);
  }

  /**
   * Post a message to the PPAPI plugin, for cases where direct response is
   * expected from the PPAPI plugin.
   * @param {!MessageData} message
   * @return {!Promise} A promise holding the response from the PPAPI plugin.
   * @private
   */
  postMessageWithReply_(message) {
    const promiseResolver = new PromiseResolver();
    message.messageId = `${message.type}_${this.createUid_()}`;
    this.requestResolverMap_.set(message.messageId, promiseResolver);
    this.postMessage_(message);
    return promiseResolver.promise;
  }


  /** @override */
  rotateClockwise() {
    this.postMessage_({type: 'rotateClockwise'});
  }

  /** @override */
  rotateCounterclockwise() {
    this.postMessage_({type: 'rotateCounterclockwise'});
  }

  /** @override */
  setDisplayAnnotations(displayAnnotations) {
    this.postMessage_({
      type: 'displayAnnotations',
      display: displayAnnotations,
    });
  }

  /** @override */
  setTwoUpView(enableTwoUpView) {
    this.postMessage_({
      type: 'setTwoUpView',
      enableTwoUpView: enableTwoUpView,
    });
  }

  /** @override */
  print() {
    this.postMessage_({type: 'print'});
  }

  selectAll() {
    this.postMessage_({type: 'selectAll'});
  }

  getSelectedText() {
    return this.postMessageWithReply_({type: 'getSelectedText'});
  }

  /**
   * Post a thumbnail request message to the plugin.
   * @param {number} page
   * @return {!Promise<!ThumbnailMessageData>} A promise holding the thumbnail
   *     response from the plugin.
   */
  requestThumbnail(page) {
    return this.postMessageWithReply_({
      type: 'getThumbnail',
      // The plugin references pages using zero-based indices.
      page: page - 1,
    });
  }

  /** @param {!PrintPreviewParams} printPreviewParams */
  resetPrintPreviewMode(printPreviewParams) {
    this.postMessage_({
      type: 'resetPrintPreviewMode',
      url: printPreviewParams.url,
      grayscale: printPreviewParams.grayscale,
      // If the PDF isn't modifiable we send 0 as the page count so that no
      // blank placeholder pages get appended to the PDF.
      pageCount:
          (printPreviewParams.modifiable ?
               printPreviewParams.pageNumbers.length :
               0)
    });
  }

  /**
   * @param {number} color New color, as a 32-bit integer, of the PDF plugin
   *     background.
   */
  setBackgroundColor(color) {
    this.postMessage_({
      type: 'setBackgroundColor',
      color: color,
    });
  }

  /**
   * @param {string} url
   * @param {number} index
   */
  loadPreviewPage(url, index) {
    this.postMessage_({type: 'loadPreviewPage', url: url, index: index});
  }

  /** @param {string} password */
  getPasswordComplete(password) {
    this.postMessage_({type: 'getPasswordComplete', password: password});
  }

  /**
   * @param {string} destination
   * @return {!Promise<!NamedDestinationMessageData>}
   *     A promise holding the named destination information from the plugin.
   */
  getNamedDestination(destination) {
    return this.postMessageWithReply_({
      type: 'getNamedDestination',
      namedDestination: destination,
    });
  }

  /** @param {boolean} enableReadOnly */
  setReadOnly(enableReadOnly) {
    this.postMessage_({
      type: 'setReadOnly',
      enableReadOnly: enableReadOnly,
    });
  }

  /** @override */
  save(requestType) {
    const resolver = new PromiseResolver();
    const newToken = createToken();
    this.pendingTokens_.set(newToken, resolver);
    this.postMessage_({
      type: 'save',
      token: newToken,
      saveRequestType: requestType,
    });
    return resolver.promise;
  }

  /** @override */
  saveAttachment(index) {
    return this.postMessageWithReply_({
      type: 'saveAttachment',
      attachmentIndex: index,
    });
  }

  /** @override */
  async load(fileName, data) {
    const url = URL.createObjectURL(new Blob([data]));
    this.plugin_.removeAttribute('headers');
    this.plugin_.setAttribute('stream-url', url);
    this.plugin_.setAttribute('has-edits', '');
    this.plugin_.style.display = 'block';
    try {
      await this.getLoadedCallback_();
      this.isActive = true;
    } finally {
      URL.revokeObjectURL(url);
    }
  }

  /** @override */
  unload() {
    this.plugin_.style.display = 'none';
    this.isActive = false;
  }

  /**
   * An event handler for handling message events received from the plugin.
   * @param {!Event} messageEvent a message event.
   * @private
   */
  handlePluginMessage_(messageEvent) {
    const messageData = /** @type {!MessageData} */ (messageEvent.data);

    // Handle case where this Plugin->Page message is a direct response
    // to a previous Page->Plugin message
    if (messageData.messageId !== undefined) {
      const resolver =
          this.requestResolverMap_.get(messageData.messageId) || null;
      assert(resolver !== null);
      this.requestResolverMap_.delete(messageData.messageId);
      resolver.resolve(messageData);
      return;
    }

    switch (messageData.type) {
      case 'goToPage':
        this.viewport_.goToPage(
            /** @type {{type: string, page: number}} */ (messageData).page);
        break;
      case 'setScrollPosition':
        this.viewport_.scrollTo(/** @type {!PartialPoint} */ (messageData));
        break;
      case 'scrollBy':
        this.viewport_.scrollBy(/** @type {!Point} */ (messageData));
        break;
      case 'saveData':
        this.saveData_(/** @type {!SaveDataMessageData} */ (messageData));
        break;
      case 'consumeSaveToken':
        const saveTokenData =
            /** @type {{ type: string, token: string }} */ (messageData);
        const resolver = this.pendingTokens_.get(saveTokenData.token);
        assert(this.pendingTokens_.delete(saveTokenData.token));
        resolver.resolve(null);
        break;
      default:
        this.eventTarget_.dispatchEvent(new CustomEvent(
            PluginControllerEventType.PLUGIN_MESSAGE, {detail: messageData}));
    }
  }

  /**
   * Handles the pdf file buffer received from the plugin.
   * @param {!SaveDataMessageData} messageData data of the message event.
   * @private
   */
  saveData_(messageData) {
    // Verify a token that was created by this instance is included to avoid
    // being spammed.
    const resolver = this.pendingTokens_.get(messageData.token);
    assert(this.pendingTokens_.delete(messageData.token));

    if (!messageData.dataToSave) {
      resolver.reject();
      return;
    }

    // Verify the file size and the first bytes to make sure it's a PDF. Cap at
    // 100 MB. This cap should be kept in sync with and is also enforced in
    // pdf/out_of_process_instance.cc.
    const MIN_FILE_SIZE = '%PDF1.0'.length;
    const MAX_FILE_SIZE = 100 * 1000 * 1000;

    const buffer = messageData.dataToSave;
    const bufView = new Uint8Array(buffer);
    assert(
        bufView.length <= MAX_FILE_SIZE,
        `File too large to be saved: ${bufView.length} bytes.`);
    assert(bufView.length >= MIN_FILE_SIZE);
    assert(
        String.fromCharCode(bufView[0], bufView[1], bufView[2], bufView[3]) ===
        '%PDF');

    resolver.resolve(messageData);
  }
}

addSingletonGetter(PluginController);
