// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import type {PageContentNode} from './ai_overlay_dialog.mojom-webui.js';
import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './ai_overlay_dialog.mojom-webui.js';
import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import type {AudioCapturer} from './audio_capturer.js';
import {BlobAudioCapturer, MicrophoneAudioCapturer} from './audio_capturer.js';
import {AudioPlayer} from './audio_player.js';
import {CaptionBlockManager, formatCaptions} from './caption_block_manager.js';
import {LocalSpeechRecognition} from './local_speech_recognition.js';
// <if expr="_google_chrome">
import {Conversation, DEFAULT_TTC_BUNDLE_URL, State} from './internal/conversation.js';
import type {ApiConfig, ConversationConfig, Persona} from './internal/conversation.js';
export type ConversationMessage =|{
  type: 'inputTranscription',
  text: string,
}|{
  type: 'outputTranscription',
  text: string,
}|{type: 'clearTranscription'};
// </if>
// TODO(crbug.com/532098288): Replace `any` types with proper interfaces when sharing types between branded and unbranded builds.
/* eslint-disable @typescript-eslint/no-explicit-any */
// Empty stub implementation of Conversation and State for non-branded (Unbranded Chromium) builds
// where internal resources are omitted.
// <if expr="not _google_chrome">
enum State {
  STOPPED = 'stopped',
  LISTENING = 'listening',
  THINKING = 'thinking',
  TALKING = 'talking',
  ERROR = 'error'
}
class Conversation {
  connected: boolean = false;
  pageContext: any = null;
  constructor(
      _config: any, _callbacks: {
        sendToUI: (msg: any) => void,
        onStateChange: (state: any, oldState: any) => void,
        onResponse: (audioData: any) => void,
      },
      _tools?: any, _router?: any, _context?: any, _pageHandler?: any) {}
  sendAudio(..._args: any[]): void {}
  sendText(..._args: any[]): void {}
  markMockAudioEndTime(..._args: any[]): void {}
  onTranscription(..._args: any[]): void {}
  onTurnComplete(): void {}
  interrupt(): void {}
  recordOnDeviceSpeechTranscript(..._args: any[]): void {}
  start(): Promise<void> { return Promise.resolve(); }
  stop(): void {}
}
type ApiConfig = any;
type ConversationConfig = {
  persona?: any,
  system_instruction?: string,
  api_config?: any,
  [key: string]: any,
};
type ConversationMessage = any;
type Persona = any;
const DEFAULT_TTC_BUNDLE_URL = '';
// </if>
/* eslint-enable @typescript-eslint/no-explicit-any */
import {errorLog, log} from './logging.js';
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
      inputTranscription: {
        type: String,
      },
      outputTranscription: {
        type: String,
      },
      inputCaptionsVisible: {
        type: Boolean,
      },
      outputCaptionsVisible: {
        type: Boolean,
      },
      activeType: {
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
      state: {
        type: String,
      },
      usePersona: {
        type: Boolean,
      },
    };
  }

  protected accessor initializationState = InitializationState.UNINITIALIZED;
  protected accessor state = State.LISTENING;
  protected accessor mockButtons: MockAudioButton[] = [];
  protected accessor sequences: Sequence[] = [];
  protected accessor inputTranscription: string = '';
  protected accessor outputTranscription: string = '';
  protected accessor inputCaptionsVisible: boolean = true;
  protected accessor outputCaptionsVisible: boolean = true;
  protected accessor activeType: 'input'|'output' = 'output';
  protected accessor speakingBlobUrl: string = '';
  protected accessor listeningBlobUrl: string = '';
  protected accessor usePersona: boolean = true;

  private isLocalTranscription: boolean = false;
  private uiStateListeningTimeout: number = 0;
  private audioPlaybackStartTime: number = 0;

  private localSpeechRecognition = new LocalSpeechRecognition({
    onLocalTranscript: (text: string) => this.onLocalInputTranscription(text),
    isAssistantSpeaking: () => this.isAssistantSpeaking(),
    isSessionListening: () =>
        Boolean(this.conversation?.connected) && this.state === State.LISTENING,
  });

  private captionBlockManager = new CaptionBlockManager({
    onCaptionBlockUpdated: (blockText: string) => {
      this.outputTranscription = blockText;
    },
  });

  protected isAssistantSpeaking(): boolean {
    return this.state === State.TALKING ||
        (this.audioPlayer?.isPlaying() ?? false);
  }

  protected shouldHideCaptionsContainer(): boolean {
    if (this.isAssistantSpeaking()) {
      return !this.outputTranscription || !this.outputCaptionsVisible;
    }
    return !this.inputTranscription || !this.inputCaptionsVisible;
  }

  protected activeCaptionText(): string {
    const text = this.isAssistantSpeaking() ? this.outputTranscription :
                                              this.inputTranscription;
    return this.formatCaptionsText(text);
  }

  protected isOutputCaptionActive(): boolean {
    return this.isAssistantSpeaking() && this.outputCaptionsVisible;
  }

  protected isInputCaptionActive(): boolean {
    return !this.isAssistantSpeaking() && this.inputCaptionsVisible;
  }

  protected formatCaptionsText(text: string): string {
    return formatCaptions(text, this.usePersona ? 50 : 60);
  }



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
        (url, title) => this.initialPageContext =
            {url, title, content: null, hasHadContent: false});
    const updateContextId =
        this.pageCallbackRouter.updateCurrentPageContext.addListener(
            (title: string, rootNode: PageContentNode|null) => {
              if (this.initialPageContext) {
                this.initialPageContext.title = title;
                this.initialPageContext.content = rootNode ?? null;
                this.initialPageContext.hasHadContent ||= Boolean(rootNode);
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

    this.pageCallbackRouter.setInputCaptionsVisible.addListener(visible => {
      log(FILE, `setInputCaptionsVisible: ${visible}`);
      this.inputCaptionsVisible = visible;
      this.requestUpdate();
    });

    this.pageCallbackRouter.setOutputCaptionsVisible.addListener(visible => {
      log(FILE, `setOutputCaptionsVisible: ${visible}`);
      this.outputCaptionsVisible = visible;
      this.requestUpdate();
    });

    this.pageCallbackRouter.setUsePersona.addListener(usePersona => {
      log(FILE, `setUsePersona: ${usePersona}`);
      if (this.usePersona === usePersona) {
        return;
      }
      this.usePersona = usePersona;
      this.requestUpdate();
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
    window.addEventListener('focus', this.onWindowFocus);
    if (document.visibilityState === 'visible') {
      this.startConversation();
    }
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    document.removeEventListener('visibilitychange', this.onVisibilityChange);
    window.removeEventListener('focus', this.onWindowFocus);
    if (this.speakingBlobUrl) {
      URL.revokeObjectURL(this.speakingBlobUrl);
    }
    if (this.listeningBlobUrl) {
      URL.revokeObjectURL(this.listeningBlobUrl);
    }
  }

  override updated(_changedProperties: PropertyValues<this>) {
    super.updated(_changedProperties);
    const changed = _changedProperties as Map<PropertyKey, unknown>;
    if (changed.has('inputTranscription')) {
      const bubble = this.shadowRoot?.querySelector('.caption-bubble.input');
      if (bubble) {
        bubble.scrollTop = bubble.scrollHeight;
      }
    }
    if (changed.has('outputTranscription')) {
      const bubble = this.shadowRoot?.querySelector('.caption-bubble.output');
      if (bubble) {
        bubble.scrollTop = bubble.scrollHeight;
      }
    }
  }

  private onVisibilityChange = () => {
    if (document.visibilityState === 'visible') {
      this.startConversation();
    } else {
      this.stopConversation();
    }
  };

  private onWindowFocus = () => {
    if (document.visibilityState === 'visible' &&
        this.state === State.LISTENING && this.conversation?.connected) {
      this.localSpeechRecognition.start();
    }
  };

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
          sendToUI: (msg: ConversationMessage) =>
              this.onMessageFromConversation(msg),
          onStateChange: (state) => this.onConversationStateChanged(state),
          onResponse: (audioData) => this.onAudioOutput(audioData),
        },
        this.toolsRemote, this.pageCallbackRouter, this.initialPageContext,
        this.pageHandler);

    if (this.unregisterPageContextListeners) {
      this.unregisterPageContextListeners();
    }

    return conversation;
  }

  private energyAnimationLoop = () => {
    if (this.energyAnimationId === null) {
      return;
    }

    let energy = 0;
    if (this.state === State.TALKING && this.audioPlayer) {
      energy = this.audioPlayer.getEnergy();
    } else if (this.state === State.LISTENING && this.audioCapturer) {
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
      this.localSpeechRecognition.stop();
      this.onLocalInputTranscription(button.text);
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
    this.localSpeechRecognition.stop();
    // Simulate local speech recognition using the button's name/text
    if (button.text) {
      this.onLocalInputTranscription(button.text);
    } else {
      this.onLocalInputTranscription(button.name);
    }
    this.blobCapturer.send(blob).then(() => {
      this.conversation?.markMockAudioEndTime(performance.now());
    });
  }

  private executeSequence(sequence: Sequence) {
    let delay = 0;
    for (const item of sequence.buttons) {
      if (typeof item === 'number') {
        delay += item;
      } else if (typeof item === 'string') {
        const index = this.mockButtons.findIndex(b => b.name === item);
        if (index !== -1) {
          window.setTimeout(() => this.runButtonByIndex(index), delay);
        } else {
          log(FILE, `Sequence button not found: ${item}`);
        }
      }
    }
  }

  private injectAudio(button: MockAudioButton) {
    const index = this.mockButtons.indexOf(button);
    if (index !== -1) {
      this.runButtonByIndex(index);
    }
  }

  protected onInjectAudioClick = (e: Event) => {
    const target = e.currentTarget as HTMLElement;
    const index = Number(target.dataset['index']);
    const button = this.mockButtons[index];
    if (button) {
      this.injectAudio(button);
    }
  };

  protected onTextInputKeydown = (e: KeyboardEvent) => {
    if (e.key === 'Enter') {
      const input = e.target as HTMLInputElement;
      const text = input.value.trim();
      if (text) {
        this.localSpeechRecognition.stop();
        log(FILE, `Injecting text: ${text}`);
        this.conversation?.sendText(text);
        input.value = '';
      }
    }
  };

  protected onSequenceClick = (e: Event) => {
    const index = Number((e.currentTarget as HTMLElement).dataset['index']);
    const sequence = this.sequences[index];
    if (!sequence) {
      return;
    }

    log(FILE, `Running sequence: ${sequence.name}`);
    this.executeSequence(sequence);
  };

  private createAudioPlayer(): AudioPlayer {
    const onStart = () => {
      clearTimeout(this.uiStateListeningTimeout);
      this.state = State.TALKING;
      this.audioPlaybackStartTime = Date.now();
      log(FILE, `AudioPlayer: Started playing audio`);
    };

    const onDone = () => {
      clearTimeout(this.uiStateListeningTimeout);

      // Calculate the remaining delay using our dynamic accessibility formula:
      // Ensure the last block is visible for at least 6 seconds (4s reading +
      // 2s additional) and there is at least a 2-second comfortable cooldown
      // after the audio stops.
      const elapsed = this.captionBlockManager.getLastBlockSwitchTime() ?
          (Date.now() - this.captionBlockManager.getLastBlockSwitchTime()) :
          0;
      const delayMs = Math.max(2000, 6000 - elapsed);

      log(FILE,
          `AudioPlayer: Finished playing audio. Elapsed since last block: ${
              elapsed}ms. Scheduling LISTENING transition in ${delayMs}ms`);

      this.uiStateListeningTimeout = setTimeout(() => {
        log(FILE,
            `LISTENING transition timer fired. Setting state to LISTENING`);
        if (this.state === State.TALKING) {
          this.state = State.LISTENING;
        }
        this.inputTranscription = '';
        this.outputTranscription = '';  // Clear output captions!
        this.captionBlockManager.reset();
        this.isLocalTranscription = false;
      }, delayMs);
    };

    return new AudioPlayer(onStart, onDone);
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
      log(FILE,
          'Mock audio buttons detected. Using BlobAudioCapturer and ' +
              'skipping physical microphone.');
      return this.blobCapturer;
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
      const ttcBundleUrl =
          (loadTimeData.valueExists('ttcBundleUrl') ?
               loadTimeData.getString('ttcBundleUrl') :
               '') ||
          DEFAULT_TTC_BUNDLE_URL;
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
    log(FILE, `stopConversation: Stopping conversation and audio capture...`);
    this.stopEnergyAnimation();
    this.mockButtons = [];
    this.blobCapturer = null;
    this.audioCapturer?.stop();
    this.audioPlayer?.stop();
    this.audioCapturer = null;
    this.audioPlayer = null;

    this.localSpeechRecognition.stop();
    this.isLocalTranscription = false;

    this.initializationState = InitializationState.UNINITIALIZED;
  }

  private onConversationStateChanged(state: State) {
    log(FILE, `onConversationStateChanged: transitioned to ${state}`);
    this.state = state;

    if (state === State.STOPPED) {
      this.stopConversation();
    } else if (state === State.LISTENING) {
      this.audioPlayer?.stop();
      clearTimeout(this.uiStateListeningTimeout);
      this.inputTranscription = '';
      this.outputTranscription = '';
      this.captionBlockManager.reset();
      this.isLocalTranscription = false;
      this.localSpeechRecognition.start();
    } else if (state === State.TALKING) {
      clearTimeout(this.uiStateListeningTimeout);
      this.inputTranscription = '';
      log(FILE,
          'onConversationStateChanged: transitioned to TALKING. ' +
              'Preserving caption blocks.');
    }
  }

  private onMessageFromConversation(msg: ConversationMessage) {
    if (msg.type === 'outputTranscription') {
      log(FILE,
          `onMessageFromConversation: Received outputTranscription (length: ${
              msg.text.length})`);
      clearTimeout(this.uiStateListeningTimeout);
      if (this.state !== State.TALKING) {
        this.state = State.TALKING;
      }
      this.inputTranscription = '';
      this.activeType = 'output';

      this.captionBlockManager.updateBlocks(
          msg.text, this.usePersona, Boolean(this.audioPlayer?.isPlaying()),
          this.audioPlaybackStartTime);
    } else if (msg.type === 'clearTranscription') {
      log(FILE,
          `onMessageFromConversation: Received clearTranscription (state: ${
              this.state})`);
      if (this.state !== State.TALKING && !(this.audioPlayer?.isPlaying())) {
        this.captionBlockManager.reset();
        this.inputTranscription = '';
        this.outputTranscription = '';
      }
    }
  }

  private onLocalInputTranscription(text: string) {
    this.inputTranscription = text;
    this.isLocalTranscription = true;
    this.activeType = 'input';

    if (this.conversation) {
      this.conversation.recordOnDeviceSpeechTranscript(text);
    }

    clearTimeout(this.transcriptionTimeout);
    this.transcriptionTimeout = window.setTimeout(() => {
      if (this.isLocalTranscription && this.inputTranscription === text) {
        log(FILE, `Clearing stale local transcription after 3 seconds.`);
        this.inputTranscription = '';
      }
    }, 3000);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ai-overlay-dialog-app': AppElement;
  }
}

customElements.define(AppElement.is, AppElement);
