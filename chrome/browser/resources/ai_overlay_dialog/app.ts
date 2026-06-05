// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './ai_overlay_dialog.mojom-webui.js';
import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import type {AudioCapturer} from './audio_capturer.js';
import {BlobAudioCapturer, MicrophoneAudioCapturer} from './audio_capturer.js';
import {AudioPlayer} from './audio_player.js';
import {Conversation, State} from './conversation.js';
import type {ApiConfig, ConversationConfig, OutputTranscriptionMessage, Persona} from './conversation.js';
import {errorLog, log, warnLog} from './logging.js';
import type {PageContext} from './page_context_manager.js';
import {AiOverlayToolsRemote} from './tools.mojom-webui.js';

const FILE = 'App';

/**
 * Used to describe the phase of the app during startup.
 */
enum InitializationState {
  UNINITIALIZED = 'uninitialized',
  CONNECTING = 'connecting',
  ERROR = 'error',
  INITIALIZED = 'initialized',
}

/**
 * Used to describe the state of the app after InitializationState reaches
 * INITIALIZED.
 */
enum UiState {
  LISTENING = 'listening',
  SPEAKING = 'speaking',
}

interface MockAudioButton {
  name: string;
  wavdata?: string;
  text?: string;
}

interface Sequence {
  name: string;
  buttons: Array<string|number>;
}

interface PersonaConfig {
  personas: Persona[];
}

interface ResourceBundle {
  persona: Persona;
  apiConfig: ApiConfig;
  speakingBlob: Blob;
  listeningBlob: Blob;
  instruction: string;
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
      sequences: {
        type: Array,
      },
      transcription: {
        type: String,
      },
      speakingBlobUrl: {
        type: String,
      },
      listeningBlobUrl: {
        type: String,
      },
      initializationState: {
        type: String,
      },
      uiState: {
        type: String,
      },
      captionsVisible: {
        type: Boolean,
      },
      usePersona: {
        type: Boolean,
      },
    };
  }

  protected accessor initializationState = InitializationState.UNINITIALIZED;
  protected accessor uiState = UiState.LISTENING;
  protected accessor mockButtons: MockAudioButton[] = [];
  protected accessor sequences: Sequence[] = [];
  protected accessor transcription: string = '';
  protected accessor speakingBlobUrl: string = '';
  protected accessor listeningBlobUrl: string = '';
  protected accessor captionsVisible: boolean = true;
  protected accessor usePersona: boolean = false;

  private pageHandler: PageHandlerRemote;
  private toolsRemote: AiOverlayToolsRemote;
  private pageCallbackRouter: PageCallbackRouter;
  private conversation: Conversation|null = null;
  private blobCapturer: BlobAudioCapturer|null = null;
  private audioCapturer: AudioCapturer|null = null;
  private audioPlayer: AudioPlayer|null = null;
  // The conversation and thus the page context manager take some time to
  // initialize so keep track of any page context that arrives before those are
  // setup so that it can be provided when these objects initialize.
  private initialPageContext?: PageContext;
  private unregisterPageContextListeners: (() => void)|null;
  private transcriptionTimeout: number = 0;
  private energyAnimationId: number|null = null;

  constructor() {
    super();

    // Setup Mojo connection
    this.pageCallbackRouter = new PageCallbackRouter();
    this.pageHandler = new PageHandlerRemote();
    this.toolsRemote = new AiOverlayToolsRemote();

    // Start listening for page context updates immediately to ensure we catch
    // any initial updates before the Conversation is initialized.
    const didChangePageId = this.pageCallbackRouter.didChangePage.addListener(
        (url, title, content) => this.initialPageContext =
            {url, title, content, hasHadContent: (content?.length ?? 0) > 0});
    const updateContextId =
        this.pageCallbackRouter.updateCurrentPageContext.addListener(
            (title, content) => {
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

    this.pageCallbackRouter.setCaptionsVisible.addListener(visible => {
      log(FILE, `setCaptionsVisible: ${visible}`);
      this.captionsVisible = visible;
    });

    this.pageCallbackRouter.setUsePersona.addListener(usePersona => {
      log(FILE, `setUsePersona: ${usePersona}`);
      if (this.usePersona === usePersona) {
        return;
      }
      this.usePersona = usePersona;
      if (this.conversation?.connected || this.initializationState === InitializationState.ERROR) {
        log(FILE, 'Restarting conversation for persona change');
        // TODO(gklassen): Make it so that conversation can trigger and block on
        // an initial page context, instead of pulling it out of the old
        // conversation or having AppElement proxy it during initialization.
        this.initialPageContext = this.conversation?.pageContext ?? undefined;
        this.stopConversation();
        this.conversation = null;
        this.initializationState = InitializationState.UNINITIALIZED;
        this.startConversation();
      }
    });

    const factory = PageHandlerFactory.getRemote();
    factory.createPageHandler(
        this.pageHandler.$.bindNewPipeAndPassReceiver(),
        this.pageCallbackRouter.$.bindNewPipeAndPassRemote(),
        this.toolsRemote.$.bindNewPipeAndPassReceiver());
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
    if (this.speakingBlobUrl) {
      URL.revokeObjectURL(this.speakingBlobUrl);
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

  private energyAnimationLoop = () => {
    if (this.energyAnimationId === null) {
      return;
    }

    let energy = 0;
    if (this.uiState === UiState.SPEAKING && this.audioPlayer) {
      energy = this.audioPlayer.getEnergy();
    } else if (this.uiState === UiState.LISTENING && this.audioCapturer) {
      energy = this.audioCapturer.getEnergy();
    }

    this.pageHandler.updateAudioEnergy(energy);

    this.energyAnimationId = requestAnimationFrame(this.energyAnimationLoop);
  };

  private startEnergyAnimation() {
    if (this.energyAnimationId === null) {
      this.energyAnimationId = requestAnimationFrame(this.energyAnimationLoop);
    }
  }

  private stopEnergyAnimation() {
    if (this.energyAnimationId !== null) {
      cancelAnimationFrame(this.energyAnimationId);
      this.energyAnimationId = null;
      this.pageHandler.updateAudioEnergy(0.0);
    }
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

  private runButtonByIndex(index: number) {
    const button = this.mockButtons[index];
    if (!button) {
      return;
    }

    if (button.text) {
      log(FILE, `Injecting text: ${button.name}, text: ${button.text}`);
      this.conversation?.sendText(button.text);
      return;
    }

    if (!this.blobCapturer || !button.wavdata) {
      return;
    }

    log(FILE,
        `Injecting audio: ${button.name}, length: ${button.wavdata.length}`);
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

  protected onInjectAudioClick = (e: Event) => {
    const index = Number((e.currentTarget as HTMLElement).dataset['index']);
    this.runButtonByIndex(index);
  };

  protected onTextInputKeydown = (e: KeyboardEvent) => {
    if (e.key === 'Enter') {
      const input = e.target as HTMLInputElement;
      const text = input.value.trim();
      if (text) {
        log(FILE, `Injecting text: ${text}`);
        this.conversation?.sendText(text);
        input.value = '';
      }
    }
  };

  protected onSequenceClick = async (e: Event) => {
    const index = Number((e.currentTarget as HTMLElement).dataset['index']);
    const sequence = this.sequences[index];
    if (!sequence) {
      return;
    }

    log(FILE, `Running sequence: ${sequence.name}`);
    for (const item of sequence.buttons) {
      if (typeof item === 'number') {
        log(FILE, `Sequence pause: ${item}s`);
        await new Promise(resolve => setTimeout(resolve, item * 1000));
        continue;
      }

      const buttonName = item;
      const buttonIndex =
          this.mockButtons.findIndex(b => b.name === buttonName);
      if (buttonIndex !== -1) {
        this.runButtonByIndex(buttonIndex);
        // Add a small default delay between commands if no explicit pause is
        // provided, to avoid overwhelming the API.
        await new Promise(resolve => setTimeout(resolve, 500));
      } else {
        warnLog(FILE, `Button not found for sequence: ${buttonName}`);
      }
    }
  };

  private createAudioPlayer(): AudioPlayer {
    return new AudioPlayer(/*onStart=*/
                           () => {
                             this.uiState = UiState.SPEAKING;
                           },
                           /*onDone=*/
                           () => {
                             this.uiState = UiState.LISTENING;
                           });
  }

  private async createAudioCapturer(): Promise<AudioCapturer|null> {
    try {
      const {jsonData} = await this.pageHandler.getMockAudioData();
      if (jsonData) {
        log(FILE,
            'Received mock audio data:', jsonData.substring(0, 100) + '...');
        try {
          const config = JSON.parse(jsonData);
          this.mockButtons = config.buttons || [];
          this.sequences = config.sequences || [];
          log(FILE,
              `Loaded ${this.mockButtons.length} mock buttons and ${
                  this.sequences.length} sequences`);
        } catch (parseError) {
          errorLog(FILE, 'Failed to parse mock audio JSON:', parseError);
        }
      }
    } catch (mojoError) {
      log(FILE, 'Failed to get mock audio data', mojoError);
    }

    if (this.mockButtons.length > 0) {
      this.blobCapturer = new BlobAudioCapturer();
    }

    try {
      const stream = await navigator.mediaDevices.getUserMedia({audio: true});
      return new MicrophoneAudioCapturer(stream);
    } catch (e) {
      log(FILE, 'No Microphone Found', e);
      return this.blobCapturer;
    }
  }

  private async startConversation() {
    if (this.initializationState === InitializationState.CONNECTING ||
        this.initializationState === InitializationState.INITIALIZED) {
      return;
    }

    this.initializationState = InitializationState.CONNECTING;

    try {
      const ttcBundleUrl = loadTimeData.getString('ttcBundleUrl');
      const bundle = await this.initializeResourceBundle(ttcBundleUrl);

      log(FILE, 'Bundle initialized');
      this.speakingBlobUrl = URL.createObjectURL(bundle.speakingBlob);
      this.listeningBlobUrl = URL.createObjectURL(bundle.listeningBlob);

      // Locally specified key overrides the fetched one.
      const apiKey =
          loadTimeData.getString('apiKey') || bundle.apiConfig.apiKey;
      const genericPersona: Persona = {
        id: 'generic',
        name: 'Chrome',
        nicknames: [],
        persona: '',
        voice: bundle.persona.voice,
      };
      const config: ConversationConfig = {
        persona: this.usePersona ? bundle.persona : genericPersona,
        system_instruction: bundle.instruction,
        api_config: {
          ...bundle.apiConfig,
          apiKey,
        },
      };

      if (!this.conversation) {
        this.conversation = this.createConversation(config);
      }

      await this.conversation.start();

      this.audioPlayer = this.createAudioPlayer();
      this.audioCapturer = await this.createAudioCapturer();
      if (this.audioCapturer) {
        this.audioCapturer.start(
            this.onAudioInput.bind(this, this.audioCapturer.getSampleRate()));
      }
      this.startEnergyAnimation();

      this.initializationState = InitializationState.INITIALIZED;
    } catch (e) {
      this.initializationState = InitializationState.ERROR;
      errorLog(FILE, 'startConversation failed: ', e);
    }
  }

  private stopConversation() {
    if (this.conversation?.connected) {
      log(FILE, 'Conversation connected, stopping it.');
      this.conversation.stop();
    }
  }

  private onConversationStateChanged(state: State, oldState: State) {
    log(FILE, `onConversationStateChanged: from ${oldState} to ${state}`);

    if (state === State.STOPPED) {
      this.stopEnergyAnimation();
      this.mockButtons = [];
      this.blobCapturer = null;
      this.audioCapturer?.stop();
      this.audioPlayer?.stop();
      this.audioCapturer = null;
      this.audioPlayer = null;

      this.initializationState = InitializationState.UNINITIALIZED;
    } else if (state === State.LISTENING) {
      this.audioPlayer?.stop();
    }
  }

  private onMessageFromConversation(msg: OutputTranscriptionMessage) {
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

    log(FILE, 'Loading resource bundle: ', baseUrl);

    const base = baseUrl.endsWith('/') ? baseUrl : baseUrl + '/';

    const signal = AbortSignal.timeout(10000);
    const [
      personaResponse,
      apiConfigResponse,
      talkingResponse,
      listeningResponse,
      instructionResponse,
    ] = await Promise.all([
      fetch(base + 'persona.json', {signal}),
      fetch(base + 'api_config.json', {signal}),
      fetch(base + 'talking.webm', {signal}),
      fetch(base + 'listening.webm', {signal}),
      fetch(base + 'instruction.tmpl', {signal}),
    ]);

    const personaConfig: PersonaConfig = await personaResponse.json();
    const apiConfig: ApiConfig = await apiConfigResponse.json();
    const speakingBlob = await talkingResponse.blob();
    const listeningBlob = await listeningResponse.blob();
    const instruction = await instructionResponse.text();

    if (!Array.isArray(personaConfig.personas) ||
        personaConfig.personas[0] === undefined) {
      throw new Error('Invalid persona config');
    }

    return {
      persona: personaConfig.personas[0],
      apiConfig,
      speakingBlob,
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
        this.toolsRemote, this.pageCallbackRouter, this.initialPageContext);

    if (this.unregisterPageContextListeners) {
      this.unregisterPageContextListeners();
    }

    return conversation;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ai-overlay-dialog-app': AppElement;
  }
}

customElements.define(AppElement.is, AppElement);
