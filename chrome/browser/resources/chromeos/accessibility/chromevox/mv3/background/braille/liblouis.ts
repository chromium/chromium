// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview JavaScript shim for the liblouis Web Assembly wrapper.
 */
import {TestImportManager} from '/common/testing/test_import_manager.js';

type LoadCallback = (instance: LibLouis) => void;
type MessageCallback = (message: Object) => void;

interface Dictionary {
  [key: string]: any;
}

/** Encapsulates a liblouis Web Assembly instance in the page. */
export class LibLouis {
  /** Path to .wasm file for the module. */
  private wasmPath_: string;
  /** Whether liblouis is loaded. */
  private isLoaded_ = false;
  /** Pending RPC callbacks. Maps from message IDs to callbacks. */
  private pendingRpcCallbacks_: {[messageId: string]: MessageCallback} = {};
  /** Next message ID to be used. Incremented with each sent message. */
  private nextMessageId_ = 1;

  worker?: Worker;

  /**
   * @param wasmPath Path to .wasm file for the module.
   * @param tablesDir Path to tables directory.
   */
  constructor(
      wasmPath: string, _tablesDir?: string, loadCallback?: LoadCallback) {
    this.wasmPath_ = wasmPath;

    this.loadOrReload_(loadCallback);
  }

  /**
   * Convenience method to wait for the constructor to resolve its callback.
   * @param wasmPath Path to .wasm file for the module.
   * @param tablesDir Path to tables directory.
   */
  static async create(wasmPath: string, tablesDir?: string): Promise<LibLouis> {
    return new Promise(resolve => new LibLouis(wasmPath, tablesDir, resolve));
  }

  isLoaded(): boolean {
    return this.isLoaded_;
  }

  /**
   * Returns a translator for the desired table, asynchronously.
   * This object must be attached to a document when requesting a translator.
   * @param {string} tableNames Comma separated list of braille table names for
   *     liblouis.
   * @return {!Promise<LibLouis.Translator>} the translator, or {@code null}
   *     on failure.
   */
  async getTranslator(tableNames: string): Promise<LibLouis.Translator|null> {
    return new Promise(resolve => {
      if (!this.isLoaded_) {
        // TODO: save last callback.
        resolve(null /* translator */);
        return;
      }
      this.rpc(
          'CheckTable', {'table_names': tableNames}, (reply: Dictionary) => {
            if (reply['success']) {
              const translator = new LibLouis.Translator(this, tableNames);
              resolve(translator);
            } else {
              resolve(null /* translator */);
            }
          });
    });
  }

  /**
   * Dispatches a message to the remote end and returns the reply
   * asynchronously. A message ID will be automatically assigned (as a
   * side-effect).
   * @param command Command name to be sent.
   * @param message JSONable message to be sent.
   * @param callback Callback to receive the reply.
   */
  rpc(command: string, message: Dictionary, callback: MessageCallback): void {
    if (!this.worker) {
      throw Error('Cannot send RPC: liblouis instance not loaded');
    }
    const messageId = '' + this.nextMessageId_++;
    message['message_id'] = messageId;
    message['command'] = command;
    const json = JSON.stringify(message);
    if (LibLouis.DEBUG) {
      globalThis.console.debug('RPC -> ' + json);
    }
    this.worker.postMessage(json);
    this.pendingRpcCallbacks_[messageId] = callback;
  }

  /** Invoked when the Web Assembly instance successfully loads. */
  private onInstanceLoad_(): void {}

  /** Invoked when the Web Assembly instance fails to load. */
  private onInstanceError_(e: ErrorEvent): void {
    globalThis.console.error('Error in liblouis ' + e.message);
    this.loadOrReload_();
  }

  /** Invoked when the Web Assembly instance posts a message. */
  private onInstanceMessage_(e: MessageEvent): void {
    if (LibLouis.DEBUG) {
      globalThis.console.debug('RPC <- ' + e.data);
    }
    const message = /** @type {!Object} */ (JSON.parse(e.data));
    const messageId = message['in_reply_to'];
    if (messageId === undefined) {
      globalThis.console.warn(
          'liblouis Web Assembly module sent message with no ID', message);
      return;
    }
    if (message['error'] !== undefined) {
      globalThis.console.error('liblouis Web Assembly error', message['error']);
    }
    const callback = this.pendingRpcCallbacks_[messageId];
    if (callback !== undefined) {
      delete this.pendingRpcCallbacks_[messageId];
      callback(message);
    }
  }

  private loadOrReload_(loadCallback?: LoadCallback): void {
    this.worker = new Worker(this.wasmPath_);
    this.worker.addEventListener(
        'message', e => this.onInstanceMessage_(e), false /* useCapture */);
    this.worker.addEventListener(
        'error', e => this.onInstanceError_(e), false /* useCapture */);
    this.rpc('load', {}, () => {
      this.isLoaded_ = true;
      loadCallback && loadCallback(this);
      this.onInstanceLoad_();
    });
  }
}

export namespace LibLouis {
  export type TranslateCallback =
      (cells: ArrayBuffer|null, textToBraille: number[]|null,
       brailleToText: number[]|null) => void;
  export type BackTranslateCallback = (text: string|null) => void;

  /**
   * Constants taken from liblouis.h.
   * Controls braille indicator insertion during translation.
   */
  export enum FormType {
    PLAIN_TEXT = 0,
    ITALIC = 1,
    UNDERLINE = 2,
    BOLD = 4,
    COMPUTER_BRAILLE = 8,
  }

  /** Set to {@code true} to enable debug logging of RPC messages. */
  export const DEBUG = false;

  /** Braille translator which uses a Web Assembly instance of liblouis. */
  export class Translator {
    private instance_: LibLouis;
    private tableNames_: string;

    /**
     * @param instance The instance wrapper.
     * @param tableNames Comma separated list of Table names to be passed to
     *     liblouis.
     */
    constructor(instance: LibLouis, tableNames: string) {
      this.instance_ = instance;
      this.tableNames_ = tableNames;
    }

    /**
     * Translates text into braille cells.
     * @param text Text to be translated.
     * @param callback Callback for result. Takes 3 parameters: the resulting
     *     cells, mapping from text to braille positions and mapping from
     *     braille to text positions. If translation fails for any reason, all
     *     parameters are null.
     */
    translate(
        text: string, formTypeMap: number[]|number,
        callback: TranslateCallback): void {
      if (!this.instance_.worker) {
        callback(
            null /*cells*/, null /*textToBraille*/, null /*brailleToText*/);
        return;
      }
      // TODO(https://crbug.com/1340093): the upstream LibLouis translations for
      // form type output is broken.
      formTypeMap = 0;
      const message = {
        'table_names': this.tableNames_,
        text,
        form_type_map: formTypeMap,
      };
      this.instance_.rpc(
          'Translate', message, (reply: {[key: string]: any}) => {
            let cells: ArrayBuffer|null = null;
            let textToBraille: number[]|null = null;
            let brailleToText: number[]|null = null;
            if (reply['success'] && typeof reply['cells'] === 'string') {
              cells = Translator.decodeHexString_(reply['cells']);
              if (reply['text_to_braille'] !== undefined) {
                textToBraille = reply['text_to_braille'];
              }
              if (reply['braille_to_text'] !== undefined) {
                brailleToText = reply['braille_to_text'];
              }
            } else if (text.length > 0) {
              // TODO(plundblad): The nacl wrapper currently returns an error
              // when translating an empty string.  Address that and always log
              // here.
              console.error(
                  'Braille translation error for ' + JSON.stringify(message));
            }
            callback(cells, textToBraille, brailleToText);
          });
    }

    /**
     * Translates braille cells into text.
     * @param cells Cells to be translated.
     * @param callback Callback for result.
     */
    backTranslate(cells: ArrayBuffer, callback: BackTranslateCallback): void {
      if (!this.instance_.worker) {
        callback(null /*text*/);
        return;
      }
      if (cells.byteLength === 0) {
        // liblouis doesn't handle empty input, so handle that trivially
        // here.
        callback('');
        return;
      }
      const message = {
        'table_names': this.tableNames_,
        'cells': Translator.encodeHexString_(cells),
      };
      this.instance_.rpc('BackTranslate', message, (reply: Dictionary) => {
        if (!reply['success'] || typeof reply['text'] !== 'string') {
          callback(null /* text */);
          return;
        }

        let text = reply['text'];

        // TODO(https://crbug.com/1340087): LibLouis has bugs in
        // backtranslation.
        const view = new Uint8Array(cells);
        if (view.length > 0 && view[view.length - 1] === 0 &&
            !text.endsWith(' ')) {
          // LibLouis omits spaces for some backtranslated contractions even
          // though it is passed a blank cell. This is a workaround until
          // LibLouis fixes this issue.
          text += ' ';
        }
        callback(text);
      });
    }

    /**
     * Decodes a hexadecimal string to an {@code ArrayBuffer}.
     * @param hex Hexadecimal string.
     * @return Decoded binary data.
     */
    private static decodeHexString_(hex: string): ArrayBuffer {
      if (!/^([0-9a-f]{2})*$/i.test(hex)) {
        throw Error('invalid hexadecimal string');
      }
      const array = new Uint8Array(hex.length / 2);
      let idx = 0;
      for (let i = 0; i < hex.length; i += 2) {
        array[idx++] = parseInt(hex.substring(i, i + 2), 16);
      }
      return array.buffer;
    }

    /**
     * Encodes an {@code ArrayBuffer} in hexadecimal.
     * @param arrayBuffer Binary data.
     * @return Hexadecimal string.
     */
    private static encodeHexString_(arrayBuffer: ArrayBuffer): string {
      const array = new Uint8Array(arrayBuffer);
      let hex = '';
      for (const b of array) {
        hex += (b < 0x10 ? '0' : '') + b.toString(16);
      }
      return hex;
    }
  }
}

TestImportManager.exportForTesting(LibLouis);
