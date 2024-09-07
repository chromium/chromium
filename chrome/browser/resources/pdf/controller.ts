// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';

// <if expr="enable_pdf_ink2">
import type {AnnotationBrush} from './constants.js';
// </if>
import type {NamedDestinationMessageData, Rect, SaveRequestType} from './constants.js';
import type {PdfPluginElement} from './internal_plugin.js';
import type {DestinationMessageData} from './pdf_viewer_utils.js';
import type {Viewport} from './viewport.js';
import {PinchPhase} from './viewport.js';

export interface MessageData {
  type: string;
  messageId?: string;
}

export interface SaveAttachmentMessageData {
  type: string;
  dataToSave: ArrayBuffer;
  messageId: string;
}

interface SaveDataMessageData {
  dataToSave: ArrayBuffer;
  token: string;
  fileName: string;
}

export interface PrintPreviewParams {
  type: string;
  url: string;
  grayscale: boolean;
  modifiable: boolean;
  pageNumbers: number[];
}

interface ThumbnailMessageData {
  imageData: ArrayBuffer;
  width: number;
  height: number;
}

// <if expr="enable_pdf_ink2">
// The message sent to the backend to set the annotation brush.
interface AnnotationBrushMessage {
  type: string;
  data: Partial<AnnotationBrush>;
}
// </if>

/**
 * Creates a cryptographically secure pseudorandom 128-bit token.
 * @return The generated token as a hex string.
 */
function createToken(): string {
  const randomBytes = new Uint8Array(16);
  window.crypto.getRandomValues(randomBytes);
  return Array.from(randomBytes, b => b.toString(16).padStart(2, '0')).join('');
}

export interface ContentController {
  isActive: boolean;

  getEventTarget(): EventTarget;
  beforeZoom(): void;
  afterZoom(): void;
  viewportChanged(): void;
  rotateClockwise(): void;
  rotateCounterclockwise(): void;
  setDisplayAnnotations(displayAnnotations: boolean): void;
  setTwoUpView(enableTwoUpView: boolean): void;

  /** Triggers printing of the current document. */
  print(): void;

  /** Undo an annotation mode edit action. */
  undo(): void;

  /** Redo an annotation mode edit action. */
  redo(): void;

  /**
   * Requests that the current document be saved.
   * @param requestType The type of save request. If ANNOTATION, a response is
   *     required, otherwise the controller may save the document to disk
   *     internally.
   */
  save(requestType: SaveRequestType): Promise<{
    fileName: string,
    dataToSave: ArrayBuffer,
    editModeForTesting?: boolean,
  }|null>;

  /**
   * Requests that the attachment at a certain index be saved.
   * @param index The index of the attachment to be saved.
   */
  saveAttachment(index: number): Promise<SaveAttachmentMessageData>;

  /** Loads PDF document from `data` activates UI. */
  load(fileName: string, data: ArrayBuffer): Promise<void>;

  /** Unloads the current document and removes the UI. */
  unload(): void;
}

/** Event types dispatched by the plugin controller. */
export enum PluginControllerEventType {
  // <if expr="enable_pdf_ink2">
  FINISH_INK_STROKE = 'PluginControllerEventType.FINISH_INK_STROKE',
  UPDATE_INK_THUMBNAIL = 'PluginControllerEventType.UPDATE_INK_THUMBNAIL',
  // </if>
  IS_ACTIVE_CHANGED = 'PluginControllerEventType.IS_ACTIVE_CHANGED',
  PLUGIN_MESSAGE = 'PluginControllerEventType.PLUGIN_MESSAGE',
}

/**
 * PDF plugin controller singleton, responsible for communicating with the
 * embedded plugin element. Dispatches a
 * `PluginControllerEventType.PLUGIN_MESSAGE` event containing the message from
 * the plugin, if a message type not handled by this controller is received.
 */
export class PluginController implements ContentController {
  private eventTarget_: EventTarget = new EventTarget();
  private isActive_: boolean = false;
  private plugin_?: PdfPluginElement;
  private delayedMessages_: Array<{message: any, transfer?: Transferable[]}>|
      null = [];
  private viewport_?: Viewport;
  private getIsUserInitiatedCallback_: () => boolean = () => false;
  private getLoadedCallback_?: () => Promise<void>| null;
  private pendingTokens_:
      Map<string,
          PromiseResolver<{fileName: string, dataToSave: ArrayBuffer}|null>> =
          new Map();
  private requestResolverMap_: Map<string, PromiseResolver<any>> = new Map();
  private uidCounter_: number = 1;

  init(
      plugin: HTMLEmbedElement, viewport: Viewport,
      getIsUserInitiatedCallback: () => boolean,
      getLoadedCallback: () => Promise<void>| null) {
    this.viewport_ = viewport;
    this.getIsUserInitiatedCallback_ = getIsUserInitiatedCallback;
    this.getLoadedCallback_ = getLoadedCallback;
    this.pendingTokens_ = new Map();
    this.requestResolverMap_ = new Map();

    this.setPlugin_(plugin);
  }

  get isActive(): boolean {
    // Check whether `plugin_` is defined as a signal that `init()` was called.
    return !!this.plugin_ && this.isActive_;
  }

  set isActive(isActive: boolean) {
    const wasActive = this.isActive;
    this.isActive_ = isActive;
    if (this.isActive === wasActive) {
      return;
    }

    this.eventTarget_.dispatchEvent(new CustomEvent(
        PluginControllerEventType.IS_ACTIVE_CHANGED, {detail: this.isActive}));
  }

  private setPlugin_(plugin: HTMLEmbedElement) {
    this.plugin_ = plugin as PdfPluginElement;
    this.plugin_.addEventListener(
        'message', e => this.handlePluginMessage_(e as MessageEvent), false);
    if (this.delayedMessages_) {
      this.plugin_.postMessage = (message, transfer) => {
        this.delayedMessages_!.push({message, transfer});
      };
    }

    // Called only from init() which always initializes |viewport_|.
    this.viewport_!.setContent(this.plugin_);
    this.viewport_!.setRemoteContent(this.plugin_);
  }

  private createUid_(): number {
    return this.uidCounter_++;
  }

  getEventTarget() {
    return this.eventTarget_;
  }

  viewportChanged() {}

  // <if expr="enable_pdf_ink2">
  setAnnotationMode(enable: boolean) {
    this.postMessage_({
      type: 'setAnnotationMode',
      enable,
    });
  }

  setAnnotationBrush(brush: AnnotationBrush) {
    const message: AnnotationBrushMessage = {
      type: 'setAnnotationBrush',
      data: brush,
    };

    this.postMessage_(message);
  }
  // </if>

  redo() {
    // <if "enable_pdf_ink2">
    this.postMessage_({type: 'annotationRedo'});
    // </if>
  }

  undo() {
    // <if "enable_pdf_ink2">
    this.postMessage_({type: 'annotationUndo'});
    // </if>
  }

  /**
   * Notify the plugin to stop reacting to scroll events while zoom is taking
   * place to avoid flickering.
   */
  beforeZoom() {
    this.postMessage_({type: 'stopScrolling'});
    assert(this.viewport_);
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
        pinchPhase: pinchPhase,
      });
    }
  }

  /**
   * Notify the plugin of the zoom change and to continue reacting to scroll
   * events.
   */
  afterZoom() {
    assert(this.viewport_);
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
      pinchVectorY: pinchVector.y,
    });
  }

  /**
   * Post a message to the plugin. Some messages will cause an async reply to be
   * received through handlePluginMessage_().
   */
  private postMessage_<M extends MessageData>(message: M) {
    assert(this.plugin_);
    this.plugin_.postMessage(message);
  }

  /**
   * Post a message to the plugin, for cases where direct response is expected
   * from the plugin.
   * @return A promise holding the response from the plugin.
   */
  private postMessageWithReply_<T, M extends MessageData>(message: M):
      Promise<T> {
    const promiseResolver = new PromiseResolver<T>();
    message.messageId = `${message.type}_${this.createUid_()}`;
    this.requestResolverMap_.set(message.messageId, promiseResolver);
    this.postMessage_(message);
    return promiseResolver.promise;
  }

  rotateClockwise() {
    this.postMessage_({type: 'rotateClockwise'});
  }

  rotateCounterclockwise() {
    this.postMessage_({type: 'rotateCounterclockwise'});
  }

  setDisplayAnnotations(displayAnnotations: boolean) {
    this.postMessage_({
      type: 'displayAnnotations',
      display: displayAnnotations,
    });
  }

  setTwoUpView(enableTwoUpView: boolean) {
    this.postMessage_({
      type: 'setTwoUpView',
      enableTwoUpView: enableTwoUpView,
    });
  }

  print() {
    this.postMessage_({type: 'print'});
  }

  selectAll() {
    this.postMessage_({type: 'selectAll'});
  }

  getSelectedText(): Promise<{selectedText: string}> {
    return this.postMessageWithReply_({type: 'getSelectedText'});
  }

  /**
   * Post a thumbnail request message to the plugin.
   * @return A promise holding the thumbnail response from the plugin.
   */
  requestThumbnail(pageIndex: number): Promise<ThumbnailMessageData> {
    return this.postMessageWithReply_({
      type: 'getThumbnail',
      pageIndex: pageIndex,
    });
  }

  resetPrintPreviewMode(printPreviewParams: PrintPreviewParams) {
    this.postMessage_({
      type: 'resetPrintPreviewMode',
      url: printPreviewParams.url,
      grayscale: printPreviewParams.grayscale,
      // If the PDF isn't modifiable we send 0 as the page count so that no
      // blank placeholder pages get appended to the PDF.
      pageCount:
          (printPreviewParams.modifiable ?
               printPreviewParams.pageNumbers.length :
               0),
    });
  }

  /**
   * @param color New color, as a 32-bit integer, of the PDF plugin
   *     background.
   */
  setBackgroundColor(color: number) {
    this.postMessage_({
      type: 'setBackgroundColor',
      color: color,
    });
  }

  loadPreviewPage(url: string, index: number) {
    this.postMessage_({type: 'loadPreviewPage', url: url, index: index});
  }

  getPageBoundingBox(page: number): Promise<Rect> {
    return this.postMessageWithReply_({
      type: 'getPageBoundingBox',
      page,
    });
  }

  getPasswordComplete(password: string) {
    this.postMessage_({type: 'getPasswordComplete', password: password});
  }

  /**
   * @return A promise holding the named destination information from the
   *     plugin.
   */
  getNamedDestination(destination: string):
      Promise<NamedDestinationMessageData> {
    return this.postMessageWithReply_({
      type: 'getNamedDestination',
      namedDestination: destination,
    });
  }

  setPresentationMode(enablePresentationMode: boolean) {
    this.postMessage_({
      type: 'setPresentationMode',
      enablePresentationMode,
    });
  }

  save(requestType: SaveRequestType) {
    const resolver =
        new PromiseResolver<{fileName: string, dataToSave: ArrayBuffer}|null>();
    const newToken = createToken();
    this.pendingTokens_.set(newToken, resolver);
    this.postMessage_({
      type: 'save',
      token: newToken,
      saveRequestType: requestType,
    });
    return resolver.promise;
  }

  saveAttachment(index: number): Promise<SaveAttachmentMessageData> {
    return this.postMessageWithReply_({
      type: 'saveAttachment',
      attachmentIndex: index,
    });
  }

  async load(_fileName: string, data: ArrayBuffer) {
    assert(this.viewport_);
    assert(this.plugin_);
    // Load `data` into the PDF plugin. The plugin transfers the data to be
    // loaded within the inner frame.
    this.viewport_.setRemoteContent(this.plugin_);
    this.plugin_.postMessage({type: 'loadArray', dataToLoad: data}, [data]);

    this.plugin_.style.display = 'block';
    if (this.getLoadedCallback_) {
      await this.getLoadedCallback_();
    }
    this.isActive = true;
  }

  unload() {
    assert(this.plugin_);
    this.plugin_.style.display = 'none';
    this.isActive = false;
  }

  /**
   * Binds an event handler for messages received from the plugin.
   *
   * TODO(crbug.com/40189769): Remove this method when a permanent postMessage()
   * bridge is implemented for the viewer.
   */
  bindMessageHandler(port: MessagePort) {
    assert(this.delayedMessages_ !== null);
    assert(this.plugin_);
    const delayedMessages = this.delayedMessages_;
    this.delayedMessages_ = null;

    this.plugin_.postMessage = port.postMessage.bind(port);
    port.onmessage = e => this.handlePluginMessage_(e);

    for (const {message, transfer} of delayedMessages) {
      this.plugin_.postMessage(message, transfer);
    }
  }

  /**
   * An event handler for handling message events received from the plugin.
   */
  private handlePluginMessage_(messageEvent: MessageEvent) {
    const messageData = messageEvent.data;

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

    assert(this.viewport_);
    switch (messageData.type) {
      case 'ackScrollToRemote':
        this.viewport_.ackScrollToRemote(messageData);
        break;
      case 'consumeSaveToken':
        const resolver = this.pendingTokens_.get(messageData.token);
        assert(resolver);
        assert(this.pendingTokens_.delete(messageData.token));
        resolver.resolve(null);
        break;
      case 'gesture':
        this.viewport_.dispatchGesture(messageData.gesture);
        break;
      case 'goToPage':
        this.viewport_.goToPage(messageData.page);
        break;
      case 'navigateToDestination':
        const destinationData = messageData as DestinationMessageData;
        this.viewport_.handleNavigateToDestination(
            destinationData.page, destinationData.x, destinationData.y,
            destinationData.zoom);
        return;
      case 'saveData':
        this.saveData_(messageData);
        break;
      case 'scrollBy':
        this.viewport_.scrollBy(messageData);
        break;
      case 'setScrollPosition':
        this.viewport_.scrollTo(messageData);
        break;
      case 'setSmoothScrolling':
        this.viewport_.setSmoothScrolling((messageData as unknown as {
                                            smoothScrolling: boolean,
                                          }).smoothScrolling);
        return;
      case 'swipe':
        this.viewport_.dispatchSwipe(messageData.direction);
        break;
      case 'syncScrollFromRemote':
        this.viewport_.syncScrollFromRemote(messageData);
        break;
      default:
        this.eventTarget_.dispatchEvent(new CustomEvent(
            PluginControllerEventType.PLUGIN_MESSAGE, {detail: messageData}));
    }
  }

  /** Handles the pdf file buffer received from the plugin. */
  private saveData_(messageData: SaveDataMessageData) {
    // Verify a token that was created by this instance is included to avoid
    // being spammed.
    const resolver = this.pendingTokens_.get(messageData.token);
    assert(resolver);
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
        String.fromCharCode(
            bufView[0]!, bufView[1]!, bufView[2]!, bufView[3]!) === '%PDF');

    resolver.resolve(messageData);
  }

  // <if expr="enable_pdf_ink2">
  setPluginForTesting(plugin: HTMLEmbedElement) {
    this.setPlugin_(plugin);
  }
  // </if>

  static getInstance(): PluginController {
    return instance || (instance = new PluginController());
  }
}

let instance: PluginController|null = null;
