// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {OffscreenCommand, ServiceWorkerCommand} from './commands.js';

type MessageSender = chrome.runtime.MessageSender;

// The data that is sent to the offscreen document for audio decoding.
// The audioData is expected to be an array of numbers that can be converted
// to a Uint8Array.
interface DecodeRequest {
  audioData: Float32Array;
  sampleRate: number;
}

class DecodeAudioHandler {
  constructor() {
    // Handle messages from the service worker.
    chrome.runtime.onMessage.addListener(
        (message: any|undefined, _sender: MessageSender,
         sendResponse: (value: any) => void) => {
          return this.onMessage_(message, sendResponse);
        });

    this.notifyServiceWorker_();
  }

  /**
   * Let the service worker know that the offscreen document is ready.
   */
  private async notifyServiceWorker_(): Promise<void> {
    await chrome.runtime.sendMessage(
        /*extensionId=*/ undefined,
        /*message=*/ {command: ServiceWorkerCommand.READY});

    if (chrome.runtime.lastError) {
      console.error(
          'Could not send ready message to service worker: ',
          chrome.runtime.lastError.message);
    }
  }

  /**
   * Handles messages from the service worker. Returns true if the
   * `sendResponse` callback should be kept alive, false otherwise.
   */
  private onMessage_(message: any, sendResponse: (value: any) => void):
      boolean {
    switch (message.command) {
      case OffscreenCommand.DECODE_AUDIO:
        (async () => {
          const decodedData = await this.decodeAudioData_(message.request);
          sendResponse(decodedData);
        })();
        // Return true to keep the `sendResponse` callback alive, since
        // decoding audio data is asynchronous.
        return true;
      default:
        break;
    }

    // We do not need to keep any callbacks alive in this case.
    return false;
  }

  // Decodes audio data at a specific sample rate.
  private async decodeAudioData_(request: DecodeRequest):
      Promise<Float32Array|null> {
    // ArrayBuffers cannot be sent over chrome.runtime.sendMessage. The
    // service worker needs to convert it to an array of numbers. We
    // convert it back to an ArrayBuffer here.
    const audioData = new Uint8Array(request.audioData).buffer;
    const context = new AudioContext({sampleRate: request.sampleRate});
    let audioBuffer;
    try {
      audioBuffer = await context.decodeAudioData(audioData);
    } catch (e) {
      console.warn('Could not decode audio data', e);
      return null;
    } finally {
      context.close();
    }

    if (!audioBuffer) {
      return null;
    }

    return audioBuffer.getChannelData(0);
  }
}

document.addEventListener('DOMContentLoaded', () => new DecodeAudioHandler());
