// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/500394286): Replace stub handlers with real Tenji WASM calls.
export {};

interface TenjiData {
  wrapperJs: Uint8Array;
  wasm: ArrayBuffer;
}

interface InitMessage {
  type: 'init';
  tenjiData: TenjiData;
}

interface TranslateMessage {
  type: 'translate';
  text: string;
}

interface InitResponse {
  type: 'init';
  error?: string;
}

interface TranslateResponse {
  type: 'translate';
  value?: string;
  error?: string;
  textToBraille?: number[];
  brailleToText?: number[];
}

interface BackTranslateMessage {
  type: 'backTranslate';
  tenjiString: string;
}

interface BackTranslateResponse {
  type: 'backTranslate';
  value?: string;
  error?: string;
}

function initResponse(error?: string): InitResponse {
  return {type: 'init', error};
}

function translateResponse(
    value?: string, error?: string, textToBraille?: number[],
    brailleToText?: number[]): TranslateResponse {
  return {type: 'translate', value, error, textToBraille, brailleToText};
}

async function handleInitMessage_(_initMessage: InitMessage): Promise<void> {
  window.parent.postMessage(initResponse(), '*');
}

function handleTranslateMessage_(translateMessage: TranslateMessage): void {
  void translateMessage;
  window.parent.postMessage(translateResponse('', undefined, [], []), '*');
}

function handleBackTranslateMessage_(
    backTranslateMessage: BackTranslateMessage): void {
  void backTranslateMessage;
  const response: BackTranslateResponse = {type: 'backTranslate', value: ''};
  window.parent.postMessage(response, '*');
}

function handleMessage_(event: MessageEvent): void {
  if (!event.data) {
    return;
  }

  if (event.data.type === 'init') {
    void handleInitMessage_(event.data as InitMessage);
    return;
  }

  if (event.data.type === 'translate') {
    handleTranslateMessage_(event.data as TranslateMessage);
    return;
  }

  if (event.data.type === 'backTranslate') {
    handleBackTranslateMessage_(event.data as BackTranslateMessage);
    return;
  }

  console.warn(
      'Unknown message type received in sandboxed wrapper: ' + event.data.type);
}

globalThis.addEventListener('message', handleMessage_);
