// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {PageHandlerFactory, PageHandlerRemote} from './ai_overlay_dialog.mojom-webui.js';
import {SessionState} from './api_session.js';
import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import type {AudioCapturer} from './audio_capturer.js';
import {BlobAudioCapturer, MicrophoneAudioCapturer} from './audio_capturer.js';
import {AudioPlayer} from './audio_player.js';
import {Conversation} from './conversation.js';

export class AppElement extends CrLitElement {
  static get is() {
    return 'ai-overlay-dialog-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      // If a mock microphone is being used, this property is set to a callback
      // injects a pre-canned message usable for development on devices without
      // a microphone.
      onInjectPrecannedAudio_: {
        type: Object,
      },
    };
  }

  private accessor onInjectPrecannedAudio_: (() => void)|undefined;

  private pageHandler: PageHandlerRemote;
  // If onStateClick_ happens before the API key mojo returns, this will turn
  // to true and invoke the state change after the key becomes available.
  private queueStateChange: boolean = false;
  private state: SessionState = SessionState.IDLE;
  private conversation: Conversation|null = null;

  constructor() {
    super();

    // Setup Mojo connection
    this.pageHandler = new PageHandlerRemote();
    const factory = PageHandlerFactory.getRemote();
    factory.createPageHandler(this.pageHandler.$.bindNewPipeAndPassReceiver());

    this.pageHandler.getApiKey().then(({apiKey}) => {
      this.conversation = new Conversation(apiKey, {
        sendToUI: (msg) => this.onMessageFromConversation(msg),
        onStateChange: (state) => this.setState(state),
        createAudioCapturer: () => this.createAudioCapturer(),
        createAudioPlayer: () => this.createAudioPlayer(),
      });

      if (this.queueStateChange) {
        this.onStateClick_();
        this.queueStateChange = false;
      }
    });
  }

  protected get hasMic(): boolean {
    return !this.onInjectPrecannedAudio_;
  }

  protected onInjectAudioClick_() {
    this.onInjectPrecannedAudio_?.();
  }

  private createAudioPlayer(): AudioPlayer {
    return new AudioPlayer(
        /*onDone=*/ this.setState.bind(this, SessionState.LISTENING));
  }

  private async createAudioCapturer(): Promise<AudioCapturer|null> {
    try {
      const stream = await navigator.mediaDevices.getUserMedia({audio: true});
      return new MicrophoneAudioCapturer(stream);
    } catch (e) {
      console.warn('No Microphone Found', e);

      try {
        const {data} = await this.pageHandler.getMockAudioData();
        if (data) {
          const blob = new Blob([new Uint8Array(data)], {type: 'audio/wav'});
          const blobCapturer = new BlobAudioCapturer(blob);
          this.onInjectPrecannedAudio_ = () => blobCapturer.send();
          return blobCapturer;
        } else {
          console.warn('No mock audio data provided or found');
        }
      } catch (mojoError) {
        console.error('Failed to get mock audio data', mojoError);
      }
    }

    return null;
  }

  protected onStateClick_() {
    if (!this.conversation) {
      console.warn('Conversation (API key) not yet available');
      this.queueStateChange = true;
      return;
    }

    if (this.state === SessionState.IDLE) {
      console.info('Attempting to connect');
      this.conversation.start();
    } else {
      this.conversation.stop();
    }
  }

  private setState(state: SessionState) {
    if (state === this.state) {
      return;
    }

    console.info('SetState: ', state);
    this.state = state;

    if (state === SessionState.IDLE) {
      this.onInjectPrecannedAudio_ = undefined;
    }
  }

  private onMessageFromConversation(msg: any) {
    console.info('Message from conversation:', msg);
    // TODO(bokan): Handle messages like 'tool-call', 'transcription', etc.
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ai-overlay-dialog-app': AppElement;
  }
}

customElements.define(AppElement.is, AppElement);
