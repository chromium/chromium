// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './ai_overlay_dialog.mojom-webui.js';
import type {ApiConfig} from './api_session.js';
import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import type {AudioCapturer} from './audio_capturer.js';
import {BlobAudioCapturer, MicrophoneAudioCapturer} from './audio_capturer.js';
import {AudioPlayer} from './audio_player.js';
import {Conversation, State} from './conversation.js';
import type {ConversationConfig, Persona} from './conversation.js';
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

interface PersonaConfig {
  personas: Persona[];
}

interface ResourceBundle {
  persona: Persona;
  apiConfig: ApiConfig;
  talkingBlob: Blob;
  listeningBlob: Blob;
  instruction: string;
}

// TODO(bokan): Allow providing this via a switch so we can remove it.
const DEFAULT_API_CONFIG = {
  endpointUrl:
      'wss://generativelanguage.googleapis.com/ws/google.ai.generativelanguage.v1beta.GenerativeService.BidiGenerateContent',
  model: 'models/gemini-3.1-flash-live-preview',
};

const DEFAULT_PERSONA: Persona = {
  id: 'chromium',
  name: 'TheButton',
  persona: 'You are a friendly assistant that lives in a button in Chrome\'s ' +
      'overlay dialog',
  voice: 'Puck',
};

function log(msg: string, ...args: any[]) {
  console.info(`[${performance.now().toFixed(2)}] [App] ${msg}`, ...args);
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
      mockButtons: {
        type: Array,
      },
      isSpeaking: {
        type: Boolean,
      },
      isListening: {
        type: Boolean,
      },
      isConnecting: {
        type: Boolean,
      },
      transcription: {
        type: String,
      },
      talkingBlobUrl: {
        type: String,
      },
      listeningBlobUrl: {
        type: String,
      },
    };
  }

  protected accessor mockButtons: MockAudioButton[] = [];
  protected accessor isSpeaking: boolean = false;
  protected accessor isConnecting: boolean = false;
  protected accessor transcription: string = '';
  protected accessor talkingBlobUrl: string = '';
  protected accessor listeningBlobUrl: string = '';

  private pageHandler: PageHandlerRemote;
  private pageCallbackRouter: PageCallbackRouter;
  private conversation: Conversation|null = null;
  private blobCapturer: BlobAudioCapturer|null = null;
  private audioCapturer: AudioCapturer|null = null;
  private audioPlayer: AudioPlayer|null = null;
  // The conversation and thus the page context manager take some time to
  // initialize so keep track of any page context that arrives before those are
  // setup so that it can be provided when these objects initialize.
  private initialPageContext?: PageContext;
  private configPromise: Promise<ConversationConfig>;
  private unregisterPageContextListeners: (() => void)|null;
  private transcriptionTimeout: number = 0;

  protected get uiState(): UiState {
    if (this.isConnecting) {
      return UiState.CONNECTING;
    }

    if (!this.conversation?.connected) {
      return UiState.INERT;
    }

    if (this.isSpeaking) {
      return UiState.SPEAKING;
    }

    return UiState.IDLE;
  }

  protected get hasResourceBundle(): boolean {
    return !!this.talkingBlobUrl && !!this.listeningBlobUrl;
  }

  protected get useStateButton(): boolean {
    if (!this.hasResourceBundle) {
      return true;
    }

    return this.uiState === UiState.CONNECTING ||
        this.uiState === UiState.INERT;
  }

  constructor() {
    super();

    const ttcBundleUrl = loadTimeData.getString('ttcBundleUrl');
    const initializedPromise = this.initializeResourceBundle(ttcBundleUrl);
    initializedPromise.then((bundle: ResourceBundle) => {
      console.info('Blobs initialized');
      this.talkingBlobUrl = URL.createObjectURL(bundle.talkingBlob);
      this.listeningBlobUrl = URL.createObjectURL(bundle.listeningBlob);
    });
    this.configPromise = initializedPromise.then((bundle: ResourceBundle) => {
      // Locally specified key overrides the fetched one.
      const apiKey =
          loadTimeData.getString('apiKey') || bundle.apiConfig.apiKey;
      return {
        persona: bundle.persona,
        system_instruction: bundle.instruction,
        api_config: {
          ...bundle.apiConfig,
          apiKey,
        },
      };
    });
    initializedPromise.catch(e => console.error('Failed to fetch bundle: ', e));

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
    this.unregisterPageContextListeners = () => {
      // Now that the conversation is initialized, we can stop listening for
      // the initial page context.
      this.pageCallbackRouter.removeListener(didChangePageId);
      this.pageCallbackRouter.removeListener(updateContextId);
      this.initialPageContext = undefined;
      this.unregisterPageContextListeners = null;
    };

    const factory = PageHandlerFactory.getRemote();
    factory.createPageHandler(
        this.pageHandler.$.bindNewPipeAndPassReceiver(),
        this.pageCallbackRouter.$.bindNewPipeAndPassRemote());
  }

  override connectedCallback() {
    super.connectedCallback();
    document.addEventListener('visibilitychange', this.onVisibilityChange);
    if (document.visibilityState === 'visible') {
      this.startConversation();
    }
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    document.removeEventListener('visibilitychange', this.onVisibilityChange);
    if (this.talkingBlobUrl) {
      URL.revokeObjectURL(this.talkingBlobUrl);
    }
    if (this.listeningBlobUrl) {
      URL.revokeObjectURL(this.listeningBlobUrl);
    }
  }

  private onVisibilityChange = () => {
    if (document.visibilityState === 'visible') {
      this.startConversation();
    } else {
      this.stopConversation();
    }
  };

  private onAudioInput(sampleRate: number, data: string) {
    this.conversation?.sendAudio(sampleRate, data);
  }

  private onAudioOutput(audioData: string) {
    // TODO(bokan): 24000 Hz (the default sampleRate in AudioPlayer) happens to
    // be what we receive from the server but we should be looking at the value
    // on the mime type and recreate the AudioPlayer if necessary.
    this.audioPlayer?.play(audioData);
  }

  protected onInjectAudioClick(e: Event) {
    if (!this.blobCapturer) {
      return;
    }

    const index = Number((e.currentTarget as HTMLElement).dataset['index']);
    const button = this.mockButtons[index];
    if (!button) {
      return;
    }

    log(`Injecting audio: ${button.name}, length: ${button.wavdata.length}`);
    const binaryString = atob(button.wavdata);
    const bytes = new Uint8Array(binaryString.length);
    for (let i = 0; i < binaryString.length; i++) {
      bytes[i] = binaryString.charCodeAt(i);
    }
    const blob = new Blob([bytes], {type: 'audio/wav'});
    this.blobCapturer.send(blob).then(() => {
      this.conversation?.markMockAudioEndTime(performance.now());
    });
  }

  private createAudioPlayer(): AudioPlayer {
    return new AudioPlayer(/*onStart=*/
                           () => {
                             this.isSpeaking = true;
                           },
                           /*onDone=*/
                           () => {
                             this.isSpeaking = false;
                           });
  }

  private async createAudioCapturer(): Promise<AudioCapturer|null> {
    try {
      const stream = await navigator.mediaDevices.getUserMedia({audio: true});
      return new MicrophoneAudioCapturer(stream);
    } catch (e) {
      log('No Microphone Found', e);

      try {
        const {jsonData} = await this.pageHandler.getMockAudioData();
        if (jsonData) {
          log('Received mock audio data:', jsonData.substring(0, 100) + '...');
          const config = JSON.parse(jsonData);
          this.mockButtons = config.buttons || [];
          log(`Loaded ${this.mockButtons.length} mock buttons`);
          this.blobCapturer = new BlobAudioCapturer();
          return this.blobCapturer;
        } else {
          log('No mock audio data provided or found');
        }
      } catch (mojoError) {
        log('Failed to get mock audio data', mojoError);
      }
    }

    return null;
  }

  private async startConversation() {
    if (this.isConnecting || this.conversation?.connected) {
      return;
    }

    if (!this.conversation) {
      this.isConnecting = true;
      let config: ConversationConfig|undefined;
      try {
        config = await this.configPromise;
      } catch (e) {
        console.warn('Using DEFAULT ApiConfig');
        config = {
          persona: DEFAULT_PERSONA,
          system_instruction: '${persona}',
          api_config: {
            ...DEFAULT_API_CONFIG,
            apiKey: loadTimeData.getString('apiKey'),
          },
        };
      }
      this.conversation = this.createConversation(config);
    }

    log('Attempting to connect. conversation state is not connected.');
    this.isConnecting = true;
    this.conversation.start().catch(e => {
      console.error('[App] Failed to start conversation:', e);
    });
  }

  private stopConversation() {
    if (this.conversation?.connected) {
      log('Conversation connected, stopping it.');
      this.conversation.stop();
    }
  }

  private async onConversationStateChanged(state: State, oldState: State) {
    log(`onConversationStateChanged: from ${oldState} to ${state}`);

    if (oldState === State.STOPPED && state !== State.STOPPED) {
      this.isConnecting = false;
      this.audioPlayer = this.createAudioPlayer();
      this.audioCapturer = await this.createAudioCapturer();
      if (this.audioCapturer) {
        this.audioCapturer.start(
            this.onAudioInput.bind(this, this.audioCapturer.getSampleRate()));
      }
    }

    if (state === State.STOPPED) {
      this.isConnecting = false;
      this.mockButtons = [];
      this.blobCapturer = null;
      this.audioCapturer?.stop();
      this.audioPlayer?.stop();
      this.audioCapturer = null;
      this.audioPlayer = null;
    } else if (state === State.LISTENING) {
      this.audioPlayer?.stop();
    }
  }

  private onMessageFromConversation(msg: any) {
    if (msg.type === 'outputTranscription') {
      this.transcription = msg.text;

      clearTimeout(this.transcriptionTimeout);
      if (this.transcription) {
        this.transcriptionTimeout = setTimeout(() => {
          this.transcription = '';
        }, 3000);
      }
    }
  }

  private async initializeResourceBundle(baseUrl: string):
      Promise<ResourceBundle> {
    if (!baseUrl) {
      throw new Error('No resource bundle URL provided');
    }

    console.info('Loading resource bundle: ', baseUrl);

    const base = baseUrl.endsWith('/') ? baseUrl : baseUrl + '/';

    const [
      personaResponse,
      apiConfigResponse,
      talkingResponse,
      listeningResponse,
      instructionResponse,
    ] = await Promise.all([
      fetch(base + 'persona.json'),
      fetch(base + 'api_config.json'),
      fetch(base + 'talking.webm'),
      fetch(base + 'listening.webm'),
      fetch(base + 'instruction.tmpl'),
    ]);

    const personaConfig: PersonaConfig = await personaResponse.json();
    const apiConfig: ApiConfig = await apiConfigResponse.json();
    const talkingBlob = await talkingResponse.blob();
    const listeningBlob = await listeningResponse.blob();
    const instruction = await instructionResponse.text();

    if (!Array.isArray(personaConfig.personas) ||
        personaConfig.personas[0] === undefined) {
      console.warn('Invalid persona config', personaConfig);
      throw new Error('Invalid persona config');
    }

    return {
      persona: personaConfig.personas[0],
      apiConfig,
      talkingBlob,
      listeningBlob,
      instruction,
    };
  }

  private createConversation(config: ConversationConfig) {
    const conversation = new Conversation(
        config, {
          sendToUI: (msg) => this.onMessageFromConversation(msg),
          onStateChange: (state, oldState) =>
              this.onConversationStateChanged(state, oldState),
          onResponse: (audioData) => this.onAudioOutput(audioData),
        },
        this.pageHandler, this.pageCallbackRouter, this.initialPageContext);

    // The conversation should only ever be created once.
    assert(this.unregisterPageContextListeners);
    this.unregisterPageContextListeners();

    return conversation;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ai-overlay-dialog-app': AppElement;
  }
}

customElements.define(AppElement.is, AppElement);
