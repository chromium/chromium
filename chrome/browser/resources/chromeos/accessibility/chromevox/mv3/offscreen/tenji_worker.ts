// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


import {ArrayBufferUtil} from '/common/array_buffer_util.js';
import {BridgeHelper} from '/common/bridge_helper.js';

import {BridgeConstants} from '../common/bridge_constants.js';

const TARGET = BridgeConstants.Offscreen.TARGET;
const Action = BridgeConstants.Offscreen.Action;

export class TenjiWorker {
  static instance?: TenjiWorker;
  private sandbox_: HTMLIFrameElement|null = null;
  private workerStarted_ = false;
  private workerStartPromise_: Promise<void>|null = null;
  private startWorkerResolver_?: () => void;
  private startWorkerRejector_?: (reason?: unknown) => void;
  private translateResolver_?: (value: {
    value: string,
    textToBraille: number[],
    brailleToText: number[]
  }) => void;
  private translateRejector_?: (reason?: unknown) => void;
  private backTranslateResolver_?: (value: string|null) => void;
  private backTranslateRejector_?: (reason?: unknown) => void;

  constructor() {
    BridgeHelper.registerHandler(
        TARGET, Action.TENJI_START_WORKER,
        (data: object) => this.startWorker_(data));
    BridgeHelper.registerHandler(
        TARGET, Action.TENJI_TRANSLATE,
        (text: string) => this.translate_(text));
    BridgeHelper.registerHandler(
        TARGET, Action.TENJI_BACK_TRANSLATE,
        (tenjiString: string) => this.backTranslate_(tenjiString));
    window.addEventListener(
        'message', (event) => this.onSandboxResponse_(event));
  }

  static init(): void {
    if (TenjiWorker.instance) {
      throw 'Error: trying to create two instances of singleton ' +
          'TenjiWorker.';
    }
    TenjiWorker.instance = new TenjiWorker();
  }

  private initializeSandbox_(): void {
    this.sandbox_ =
        document.getElementById('sandboxed-tenji-wrapper') as HTMLIFrameElement;
  }

  private async startWorker_(data: object): Promise<void> {
    if (this.workerStarted_) {
      return;
    }

    if (this.workerStartPromise_) {
      return this.workerStartPromise_;
    }

    this.workerStartPromise_ = this.doStartWorker_(data);
    try {
      await this.workerStartPromise_;
    } catch (error) {
      this.workerStartPromise_ = null;
      throw error;
    }
  }

  private async doStartWorker_(data: object): Promise<void> {
    if (!this.sandbox_) {
      this.initializeSandbox_();
    }

    if (this.startWorkerRejector_) {
      this.startWorkerRejector_('Error: startWorker request already pending.');
      this.startWorkerResolver_ = undefined;
      this.startWorkerRejector_ = undefined;
    }

    this.sandbox_!.src =
        '/chromevox/mv3/services/tenji/sandboxed_tenji_wrapper.html';
    const tenjiData: chrome.accessibilityPrivate.TenjiData = {} as
        chrome.accessibilityPrivate.TenjiData;
    tenjiData.wrapperJs =
        await ArrayBufferUtil.base64ToArrayBuffer((data as any).wrapperJs);
    tenjiData.wasm =
        await ArrayBufferUtil.base64ToArrayBuffer((data as any).wasm);

    const resultPromise = new Promise<void>((resolve, reject) => {
      this.startWorkerResolver_ = resolve;
      this.startWorkerRejector_ = reject;
    });
    this.sandbox_!.contentWindow!.postMessage({type: 'init', tenjiData}, '*');
    return resultPromise;
  }

  private async backTranslate_(tenjiString: string): Promise<string|null> {
    if (!this.sandbox_) {
      this.initializeSandbox_();
    }

    if (this.backTranslateRejector_) {
      this.backTranslateRejector_(
          'Error: backTranslate request already pending.');
      this.backTranslateResolver_ = undefined;
      this.backTranslateRejector_ = undefined;
    }

    const resultPromise = new Promise<string|null>((resolve, reject) => {
      this.backTranslateResolver_ = resolve;
      this.backTranslateRejector_ = reject;
    });
    this.sandbox_!.contentWindow!.postMessage(
        {type: 'backTranslate', tenjiString}, '*');
    return resultPromise;
  }

  private async translate_(text: string): Promise<
      {value: string, textToBraille: number[], brailleToText: number[]}> {
    if (!this.sandbox_) {
      this.initializeSandbox_();
    }

    if (this.translateRejector_) {
      this.translateRejector_('Error: translate request already pending.');
      this.translateResolver_ = undefined;
      this.translateRejector_ = undefined;
    }

    const resultPromise = new Promise<
        {value: string, textToBraille: number[], brailleToText: number[]}>(
        (resolve, reject) => {
          this.translateResolver_ = resolve;
          this.translateRejector_ = reject;
        });
    this.sandbox_!.contentWindow!.postMessage({type: 'translate', text}, '*');
    return resultPromise;
  }

  private onSandboxResponse_(event: MessageEvent): void {
    if (!this.sandbox_ || event.source !== this.sandbox_.contentWindow) {
      console.error('Reject sandbox message: bad event source');
      return;
    }

    if (!event.data || !event.data.type) {
      console.warn('Received message with no type from sandboxed wrapper.');
      return;
    }

    if (event.data.type === 'init') {
      if (event.data.error) {
        const rejector = this.startWorkerRejector_;
        this.startWorkerResolver_ = undefined;
        this.startWorkerRejector_ = undefined;
        this.workerStartPromise_ = null;
        this.workerStarted_ = false;

        if (rejector) {
          rejector(event.data.error);
        }
        return;
      }

      const resolver = this.startWorkerResolver_;
      this.startWorkerResolver_ = undefined;
      this.startWorkerRejector_ = undefined;
      this.workerStarted_ = true;

      if (resolver) {
        resolver();
      }
      return;
    }

    if (event.data.type === 'translate') {
      const resolver = this.translateResolver_;
      this.translateResolver_ = undefined;
      this.translateRejector_ = undefined;

      if (resolver) {
        resolver({
          value: event.data.value ?? '',
          textToBraille: event.data.textToBraille ?? [],
          brailleToText: event.data.brailleToText ?? []
        });
      }
      return;
    }

    if (event.data.type === 'backTranslate') {
      const resolver = this.backTranslateResolver_;
      this.backTranslateResolver_ = undefined;
      this.backTranslateRejector_ = undefined;

      if (resolver) {
        resolver(event.data.error ? null : (event.data.value ?? null));
      }
    }
  }
}
