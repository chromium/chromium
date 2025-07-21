// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestImportManager} from '/common/testing/test_import_manager.js';

import {OffscreenCommandType} from './offscreen_command_type.js';

/*
 * Helper to wrap messaging api between offscreen doc and service worker.
 */
export class Messenger {
  static readonly OFFSCREEN_DOCUMENT_PATH =
      'accessibility_common/mv3/offscreen.html';

  static instance?: Messenger;

  private readonly context_: Messenger.Context;

  // A promise that tracks when offscreen doc is ready.
  private offscreenDocumentPromise_: Promise<void>|null = null;

  // Resolves the above promise when received 'READY' from offscreen doc.
  private setOffscreenDocumentReady_?: () => void;

  // Whether a `chrome.offscreen.createDocument` is in progress.
  private offscreenDocumentCreating_: boolean = false;

  // Tracks registered message handlers.
  private registry_: Map<OffscreenCommandType, Messenger.Handler>;

  // Tracks resolvers for waitForHandled.
  private handlerResolvers_: Map<OffscreenCommandType, Array<() => void>> =
      new Map();

  constructor(context: Messenger.Context) {
    this.context_ = context;
    this.registry_ = new Map<OffscreenCommandType, Messenger.Handler>();

    chrome.runtime.onMessage.addListener(
        (message: any|undefined, _sender: chrome.runtime.MessageSender,
         sendResponse: (response?: any) => void) =>
            this.handleMessage_(message, sendResponse));
  }

  static async init(context: Messenger.Context): Promise<void> {
    if (Messenger.instance) {
      throw 'Error: trying to create two instances of singleton Messenger.';
    }
    Messenger.instance = new Messenger(context);

    if (context === Messenger.Context.OFFSCREEN) {
      Messenger.send(OffscreenCommandType.MESSENGER_SW_READY);
      return;
    }

    if (context === Messenger.Context.SERVICE_WORKER) {
      Messenger.registerHandler(OffscreenCommandType.MESSENGER_SW_READY, () => {
        Messenger.instance!.onOffscreenDocumentReady_();
      });
      return Messenger.instance.ensureOffscreenDocument_();
    }
  }

  /*
   * Ensures offscreen document is created. Returns a Promise that resolves when
   * offscreen document is created. This method should handle cases of service
   * worker restart and offscreen doc re-creation.
   */
  private async ensureOffscreenDocument_(): Promise<void> {
    const offscreenUrl =
        chrome.runtime.getURL(Messenger.OFFSCREEN_DOCUMENT_PATH);
    const existingContexts = await chrome.runtime.getContexts({
      contextTypes: [chrome.runtime.ContextType.OFFSCREEN_DOCUMENT],
      documentUrls: [offscreenUrl]
    });
    if (existingContexts.length > 0) {
      // Offscreen document is created in previous service worker runs.
      if (!this.offscreenDocumentPromise_) {
        this.offscreenDocumentPromise_ = Promise.resolve();
      }
    } else if (!this.offscreenDocumentCreating_) {
      // Otherwise, create one if there is no pending creation.
      this.offscreenDocumentCreating_ = true;
      this.offscreenDocumentPromise_ = new Promise(resolve => {
        this.setOffscreenDocumentReady_ = resolve;
      });

      chrome.offscreen
          .createDocument({
            url: offscreenUrl,
            reasons: [chrome.offscreen.Reason.WORKERS],
            justification: 'Audio web API and web assembly execution',
          })
          .catch(error => {
            console.error('Failed to create offscreen document: ', error);
            this.offscreenDocumentCreating_ = false;
          });
    }
    return this.offscreenDocumentPromise_!;
  }

  /**
   * Handles `MESSENGER_SW_READY` message from offscreen document.
   */
  private onOffscreenDocumentReady_(): void {
    this.offscreenDocumentCreating_ = false;
    this.setOffscreenDocumentReady_!();
  }

  private doSend_(command: OffscreenCommandType, data: object): Promise<any> {
    return chrome.runtime.sendMessage(
        /*extensionId=*/ undefined,
        /*message=*/ Object.assign({command}, data));
  }

  // Sends a command message to the other side. Returns a promise that resolves
  // if the other side sends back a reply.
  public static send(command: OffscreenCommandType, data = {}): Promise<any> {
    if (Messenger.instance!.context_ == Messenger.Context.OFFSCREEN) {
      return Messenger.instance!.doSend_(command, data);
    }

    return Messenger.instance!.ensureOffscreenDocument_().then(() => {
      return Messenger.instance!.doSend_(command, data);
    });
  }

  // Registers a command handler.
  static registerHandler(
      command: OffscreenCommandType, handler: Messenger.Handler): void {
    Messenger.instance!.registry_.set(command, handler);
  }

  /**
   * Test-only helper that returns a promise that resolves after the given
   * command handler has run.
   */
  static waitForHandled(command: OffscreenCommandType): Promise<void> {
    return new Promise(resolve => {
      const resolvers = Messenger.instance!.handlerResolvers_.get(command);
      if (resolvers) {
        resolvers.push(resolve);
      } else {
        Messenger.instance!.handlerResolvers_.set(command, [resolve]);
      }
    });
  }

  // Gets the handler for a given command.
  static getHandler(command: OffscreenCommandType): Messenger.Handler
      |undefined {
    return Messenger.instance!.registry_.get(command);
  }

  // Handles the command message received from the other parts of the extension.
  // For example, when running in the service worker, this handles messages from
  // the offscreen document. And when running in the offscreen document, it
  // handles messages from the service worker.
  private handleMessage_(
      message: any|undefined, sendResponse: (response?: any) => void): boolean {
    const command = message['command'];
    const result = Messenger.getHandler(command)?.(message);

    const resolvers = this.handlerResolvers_.get(command);
    if (resolvers) {
      for (const resolver of resolvers) {
        resolver();
      }
      this.handlerResolvers_.delete(command);
    }

    // If handler is async, return true to allow async sendResponse.
    if (result instanceof Promise) {
      result.then(sendResponse).catch(sendResponse);
      return true;
    }
    // Otherwise return false.
    return false;
  }
}

export namespace Messenger {
  // The context of where `Messenger` runs.
  export enum Context {
    SERVICE_WORKER = 'serviceWorker',
    OFFSCREEN = 'offscreen',
  }

  // Message handler. If a handler returns a Promise, a reply will be sent
  // to the sender when the promise resolves.
  export type Handler = (message: any|undefined) => Promise<any>|void;

  // Compression format to use. Only `gzip` and `deflate` are supported.
  const compressionFormat = 'gzip';

  async function compress(buffer: ArrayBuffer): Promise<ArrayBuffer> {
    const cs = new CompressionStream(compressionFormat);
    const readableStream = new ReadableStream({
      start(controller) {
        controller.enqueue(new Uint8Array(buffer));
        controller.close();
      }
    });

    const stream = readableStream.pipeThrough(cs);
    const response = new Response(stream);
    const blob = await response.blob();
    return await blob.arrayBuffer();
  }

  async function decompress(buffer: ArrayBuffer): Promise<ArrayBuffer> {
    const ds = new DecompressionStream(compressionFormat);
    const readableStream = new ReadableStream({
      start(controller) {
        controller.enqueue(new Uint8Array(buffer));
        controller.close();
      }
    });

    const stream = readableStream.pipeThrough(ds);
    const response = new Response(stream);
    const blob = await response.blob();
    return await blob.arrayBuffer();
  }

  // Function to convert ArrayBuffer to Base64 string
  export async function arrayBufferToBase64(inBuffer: ArrayBuffer):
      Promise<string> {
    const buffer = await compress(inBuffer);
    let binary = '';
    const bytes = new Uint8Array(buffer);
    const len = bytes.byteLength;
    for (let i = 0; i < len; i++) {
      binary += String.fromCharCode(bytes[i]);
    }
    return btoa(binary);  // btoa is a global function for Base64 encoding
  }

  // Function to convert Base64 string back to ArrayBuffer
  export async function base64ToArrayBuffer(base64: string):
      Promise<ArrayBuffer> {
    const binaryString =
        atob(base64);  // atob is a global function for Base64 decoding
    const len = binaryString.length;
    const bytes = new Uint8Array(len);
    for (let i = 0; i < len; i++) {
      bytes[i] = binaryString.charCodeAt(i);
    }
    return await decompress(bytes.buffer);
  }
}

TestImportManager.exportForTesting(Messenger);
