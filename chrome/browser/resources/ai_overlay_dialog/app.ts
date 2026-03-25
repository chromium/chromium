// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './ai_overlay_dialog.mojom-webui.js';
import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import type {AudioCapturer} from './audio_capturer.js';
import {BlobAudioCapturer, MicrophoneAudioCapturer} from './audio_capturer.js';
import {AudioPlayer} from './audio_player.js';
import {Conversation, State} from './conversation.js';
import type {PageContext} from './page_context_manager.js';

enum UiState {
  INERT = 'inert',
  CONNECTING = 'connecting',
  SPEAKING = 'speaking',
  IDLE = 'idle',
}

interface MockAudioButton {
  name: string;
  wavdata: string;
}

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
      // If a mock microphone is being used, this contains the list of buttons
      // to inject pre-canned messages.
      mockButtons_: {
        type: Array,
      },
      isSpeaking_: {
        type: Boolean,
      },
      isConnecting_: {
        type: Boolean,
      },
      transcription_: {
        type: String,
      },
    };
  }

  protected accessor mockButtons_: MockAudioButton[] = [];
  protected accessor isSpeaking_: boolean = false;
  protected accessor isConnecting_: boolean = false;
  protected accessor transcription_: string = '';

  private pageHandler: PageHandlerRemote;
  private pageCallbackRouter: PageCallbackRouter;
  // If onStateClick_ happens before the API key mojo returns, this will turn
  // to true and invoke the state change after the key becomes available.
  private queueStateChange: boolean = false;
  private conversation: Conversation|null = null;
  private blobCapturer: BlobAudioCapturer|null = null;
  private audioCapturer: AudioCapturer|null = null;
  private audioPlayer: AudioPlayer|null = null;
  // The conversation and thus the page context manager take some time to
  // initialize so keep track of any page context that arrives before those are
  // setup so that it can be provided when these objects initialize.
  private initialPageContext?: PageContext;

  protected get uiState_(): UiState {
    if (this.isConnecting_) {
      return UiState.CONNECTING;
    }

    if (!this.conversation?.connected) {
      return UiState.INERT;
    }

    if (this.isSpeaking_) {
      return UiState.SPEAKING;
    }

    return UiState.IDLE;
  }

  constructor() {
    super();

    // Setup Mojo connection
    this.pageCallbackRouter = new PageCallbackRouter();
    this.pageHandler = new PageHandlerRemote();

    // Start listening for page context updates immediately to ensure we catch
    // any initial updates before the Conversation is initialized.
    const didChangePageId = this.pageCallbackRouter.didChangePage.addListener(
        (url: string, title: string|null, content: string|null) =>
            this.initialPageContext = {url, title, content});
    const updateContextId =
        this.pageCallbackRouter.updateCurrentPageContext.addListener(
            (title: string, content: string) => {
              if (this.initialPageContext) {
                this.initialPageContext.title = title;
                this.initialPageContext.content = content;
              }
            });

    const factory = PageHandlerFactory.getRemote();
    factory.createPageHandler(
        this.pageHandler.$.bindNewPipeAndPassReceiver(),
        this.pageCallbackRouter.$.bindNewPipeAndPassRemote());

    this.pageHandler.getApiKey().then(({apiKey}: {apiKey: string}) => {
      this.conversation = new Conversation(
          apiKey, {
            sendToUI: (msg) => this.onMessageFromConversation(msg),
            onStateChange: (state, oldState) =>
                this.onConversationStateChanged(state, oldState),
            onResponse: (audioData) => this.onAudioOutput(audioData),
          },
          this.pageCallbackRouter, this.initialPageContext);

      // Now that the conversation is initialized, we can stop listening for the
      // initial page context.
      this.pageCallbackRouter.removeListener(didChangePageId);
      this.pageCallbackRouter.removeListener(updateContextId);
      this.initialPageContext = undefined;

      if (this.queueStateChange) {
        this.onStateClick_();
        this.queueStateChange = false;
      }
    });
  }

  private onAudioInput(sampleRate: number, data: string) {
    this.conversation?.sendAudio(sampleRate, data);
  }

  private onAudioOutput(audioData: string) {
    // TODO(bokan): 24000 Hz (the default sampleRate in AudioPlayer) happens to
    // be what we receive from the server but we should be looking at the value
    // on the mime type and recreate the AudioPlayer if necessary.
    this.audioPlayer?.play(audioData);
  }

  protected onInjectAudioClick_(e: Event) {
    if (!this.blobCapturer) {
      return;
    }

    const index = Number((e.currentTarget as HTMLElement).dataset['index']);
    const button = this.mockButtons_[index];
    if (!button) {
      return;
    }

    const binaryString = atob(button.wavdata);
    const bytes = new Uint8Array(binaryString.length);
    for (let i = 0; i < binaryString.length; i++) {
      bytes[i] = binaryString.charCodeAt(i);
    }
    const blob = new Blob([bytes], {type: 'audio/wav'});
    this.blobCapturer.send(blob);
  }

  private createAudioPlayer(): AudioPlayer {
    return new AudioPlayer(/*onStart=*/
                           () => {
                             this.isSpeaking_ = true;
                           },
                           /*onDone=*/
                           () => {
                             this.isSpeaking_ = false;
                           });
  }

  private async createAudioCapturer(): Promise<AudioCapturer|null> {
    try {
      const stream = await navigator.mediaDevices.getUserMedia({audio: true});
      return new MicrophoneAudioCapturer(stream);
    } catch (e) {
      console.warn('No Microphone Found', e);

      try {
        const {jsonData} = await this.pageHandler.getMockAudioData();
        if (jsonData) {
          const config = JSON.parse(jsonData);
          this.mockButtons_ = config.buttons || [];
          this.blobCapturer = new BlobAudioCapturer();
          return this.blobCapturer;
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

    if (this.isConnecting_) {
      return;
    }

    if (!this.conversation.connected) {
      console.info('Attempting to connect');
      this.isConnecting_ = true;
      this.conversation.start();
    } else {
      this.conversation.stop();
    }
  }

  private async onConversationStateChanged(state: State, oldState: State) {
    console.info('onConversationStateChanged: ', state);

    if (oldState === State.STOPPED && state !== State.STOPPED) {
      this.isConnecting_ = false;
      this.audioPlayer = this.createAudioPlayer();
      this.audioCapturer = await this.createAudioCapturer();
      if (this.audioCapturer) {
        this.audioCapturer.start(
            this.onAudioInput.bind(this, this.audioCapturer.getSampleRate()));
      }
    }

    if (state === State.STOPPED) {
      this.isConnecting_ = false;
      this.mockButtons_ = [];
      this.blobCapturer = null;
      this.audioCapturer?.stop();
      this.audioPlayer?.stop();
      this.audioCapturer = null;
      this.audioPlayer = null;
    } else if (state === State.LISTENING) {
      this.audioPlayer?.stop();
    }
  }

  private transcriptionTimeout_: number = 0;

  private onMessageFromConversation(msg: any) {
    if (msg.type === 'outputTranscription') {
      this.transcription_ = msg.text;

      clearTimeout(this.transcriptionTimeout_);
      this.transcriptionTimeout_ = setTimeout(() => {
        this.transcription_ = '';
      }, 3000);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ai-overlay-dialog-app': AppElement;
  }
}

customElements.define(AppElement.is, AppElement);
