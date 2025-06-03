// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import '//resources/cr_elements/cr_collapse/cr_collapse.js';
import '//resources/cr_elements/cr_expand_button/cr_expand_button.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/cr_elements/cr_textarea/cr_textarea.js';

import type {CrInputElement} from '//resources/cr_elements/cr_input/cr_input.js';
import {assert} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {BrowserProxy} from './browser_proxy.js';
import type {AudioData, Capabilities, InputPiece, ResponseChunk, ResponseSummary} from './on_device_model.mojom-webui.js';
import {LoadModelResult, OnDeviceModelRemote, PerformanceClass, SessionRemote, StreamingResponderCallbackRouter, Token} from './on_device_model.mojom-webui.js';
import {ModelPerformanceHint} from './on_device_model_service.mojom-webui.js';
import {getCss} from './tools.css.js';
import {getHtml} from './tools.html.js';

interface Response {
  text: string;
  response: string;
  responseClass: string;
  retracted: boolean;
  error: boolean;
}

interface OnDeviceInternalsToolsElement {
  $: {
    modelInput: CrInputElement,
    temperatureInput: CrInputElement,
    textInput: CrInputElement,
    imageInput: HTMLInputElement,
    audioInput: HTMLInputElement,
    topKInput: CrInputElement,
    performanceHintSelect: HTMLSelectElement,
  };
}

function getPerformanceClassText(performanceClass: PerformanceClass): string {
  switch (performanceClass) {
    case PerformanceClass.kVeryLow:
      return 'Very Low';
    case PerformanceClass.kLow:
      return 'Low';
    case PerformanceClass.kMedium:
      return 'Medium';
    case PerformanceClass.kHigh:
      return 'High';
    case PerformanceClass.kVeryHigh:
      return 'Very High';
    case PerformanceClass.kGpuBlocked:
      return 'GPU blocked';
    case PerformanceClass.kFailedToLoadLibrary:
      return 'Failed to load native library';
    default:
      return 'Error';
  }
}

function textToInputPieces(text: string): InputPiece[] {
  const input: InputPiece[] = [];
  for (const piece of text.split('\n')) {
    if (piece === '$SYSTEM') {
      input.push({token: Token.kSystem});
    } else if (piece === '$MODEL') {
      input.push({token: Token.kModel});
    } else if (piece === '$USER') {
      input.push({token: Token.kUser});
    } else if (piece === '$END') {
      input.push({token: Token.kEnd});
    } else if (
        input.length === 0 || input[input.length - 1]!.text === undefined) {
      input.push({text: piece});
    } else {
      input[input.length - 1]!.text += '\n' + piece;
    }
  }
  return input;
}

class OnDeviceInternalsToolsElement extends CrLitElement {
  static get is() {
    return 'on-device-internals-tools';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      modelPath_: {type: String},
      error_: {type: String},
      imageError_: {type: String},
      text_: {type: String},
      loadModelStart_: {type: Number},
      currentResponse_: {type: Object},
      responses_: {type: Array},
      model_: {type: Object},
      performanceClassText_: {type: String},
      contextExpanded_: {type: Boolean},
      contextLength_: {type: Number},
      contextText_: {type: String},
      topK_: {type: Number},
      temperature_: {type: Number},
      imageFile_: {type: Object},
      audioFile_: {type: Object},
      audioError_: {type: String},
      performanceHint_: {type: String},
      loadedPerformanceHint_: {type: Number},
    };
  }

  private capabilities_: Capabilities = {imageInput: false, audioInput: false};
  protected accessor contextExpanded_: boolean = false;
  protected accessor contextLength_: number = 0;
  protected accessor contextText_: string = '';
  protected accessor currentResponse_: Response|null = null;
  protected accessor error_: string = '';
  protected accessor imageError_: string = '';
  private loadModelDuration_: number = -1;
  private accessor loadModelStart_: number = 0;
  private accessor modelPath_: string = '';
  protected accessor model_: OnDeviceModelRemote|null = null;
  protected accessor performanceClassText_: string = 'Loading...';
  protected accessor responses_: Response[] = [];
  protected accessor temperature_: number = 0;
  protected accessor text_: string = '';
  protected accessor topK_: number = 1;
  protected accessor imageFile_: File|null = null;
  protected accessor audioFile_: File|null = null;
  protected accessor audioError_: string = '';
  protected accessor performanceHint_: string = 'kHighestQuality';
  private accessor loadedPerformanceHint_: ModelPerformanceHint|null = null;

  private session_: SessionRemote|null = null;
  private proxy_: BrowserProxy = BrowserProxy.getInstance();
  private responseRouter_: StreamingResponderCallbackRouter =
      new StreamingResponderCallbackRouter();

  override firstUpdated() {
    this.getPerformanceClass_();
    this.$.temperatureInput.inputElement.step = '0.1';
    this.$.imageInput.addEventListener(
        'change', this.onImageChange_.bind(this));
    this.$.audioInput.addEventListener(
        'change', this.onAudioChange_.bind(this));
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('model_') ||
        changedPrivateProperties.has('error_')) {
      this.onModelOrErrorChanged_();
    }
  }

  private async getPerformanceClass_() {
    this.performanceClassText_ = getPerformanceClassText(
        (await this.proxy_.handler.getEstimatedPerformanceClass())
            .performanceClass);
  }

  private onModelOrErrorChanged_() {
    if (this.model_ !== null) {
      this.loadModelDuration_ = new Date().getTime() - this.loadModelStart_;
      this.$.textInput.focus();
    }
    this.loadModelStart_ = 0;
  }

  protected onLoadClick_() {
    this.onModelSelected_();
  }

  protected onAddImageClick_() {
    this.$.imageInput.click();
  }

  protected onAddAudioClick_() {
    this.$.audioInput.click();
  }

  protected onRemoteImageClick_() {
    this.imageFile_ = null;
    this.$.imageInput.value = '';
  }

  protected onRemoteAudioClick_() {
    this.audioFile_ = null;
    this.$.audioInput.value = '';
  }


  protected onPerformanceHintChange_() {
    this.performanceHint_ = this.$.performanceHintSelect.value;
  }

  private onServiceCrashed_() {
    if (this.currentResponse_) {
      this.currentResponse_.error = true;
      this.addResponse_();
    }
    this.error_ = 'Service crashed, please reload the model.';
    this.model_ = null;
    this.modelPath_ = '';
    this.loadModelStart_ = 0;
    this.$.modelInput.focus();
  }

  private onImageChange_() {
    this.imageError_ = '';
    if ((this.$.imageInput.files?.length ?? 0) > 0) {
      this.imageFile_ = this.$.imageInput.files!.item(0) ?? null;
    } else {
      this.imageFile_ = null;
    }
  }

  private onAudioChange_() {
    this.audioError_ = '';
    if ((this.$.audioInput.files?.length ?? 0) > 0) {
      this.audioFile_ = this.$.audioInput.files!.item(0) ?? null;
    } else {
      this.audioFile_ = null;
    }
  }


  private async onModelSelected_() {
    this.error_ = '';
    if (this.model_) {
      this.model_.$.close();
    }
    if (this.model_) {
      this.model_.$.close();
    }
    this.imageFile_ = null;
    this.audioFile_ = null;
    this.model_ = null;
    this.capabilities_ = {imageInput: false, audioInput: false};
    this.loadModelStart_ = new Date().getTime();
    const performanceHint = ModelPerformanceHint[(
        this.performanceHint_ as keyof typeof ModelPerformanceHint)];
    const modelPath = this.$.modelInput.value;
    // <if expr="is_win">
    // Windows file paths are std::wstring, so use Array<Number>.
    const processedPath = Array.from(modelPath, (c) => c.charCodeAt(0));
    // </if>
    // <if expr="not is_win">
    const processedPath = modelPath;
    // </if>
    const newModel = new OnDeviceModelRemote();
    const {result, capabilities} = await this.proxy_.handler.loadModel(
        {path: processedPath}, performanceHint,
        newModel.$.bindNewPipeAndPassReceiver());
    if (result !== LoadModelResult.kSuccess) {
      this.error_ =
          'Unable to load model. Specify a correct and absolute path.';
    } else {
      this.model_ = newModel;
      this.capabilities_ = capabilities;
      this.model_.onConnectionError.addListener(() => {
        this.onServiceCrashed_();
      });
      this.startNewSession_();
      this.modelPath_ = modelPath;
      this.loadedPerformanceHint_ = performanceHint;
    }
  }

  protected onAddContextClick_() {
    if (this.session_ === null) {
      return;
    }
    this.session_.append(
        {
          maxTokens: 0,
          input: {pieces: textToInputPieces(this.contextText_)},
        },
        null);
    this.contextLength_ += this.contextText_.split(/(\s+)/).length;
    this.contextText_ = '';
  }

  protected startNewSession_() {
    if (this.model_ === null) {
      return;
    }
    this.contextLength_ = 0;
    this.session_ = new SessionRemote();
    this.model_.startSession(this.session_.$.bindNewPipeAndPassReceiver(), {
      maxTokens: 0,
      topK: this.topK_,
      temperature: this.temperature_,
      capabilities: {
        imageInput: this.imagesEnabled_(),
        audioInput: this.audioEnabled_(),
      },
    });
  }

  protected onCancelClick_() {
    this.responseRouter_.$.close();
    this.responseRouter_ = new StreamingResponderCallbackRouter();
    this.addResponse_();
  }

  protected onExecuteClick_() {
    this.onExecute_();
  }

  private async addResponse_() {
    assert(this.currentResponse_);
    this.responses_.unshift(this.currentResponse_);
    this.currentResponse_ = null;
    this.requestUpdate();
    await this.updateComplete;
    this.$.textInput.focus();
  }

  private async decodeBitmap_() {
    const data = new Uint8Array(await this.imageFile_!.arrayBuffer());
    if (data.byteLength <= 0) {
      return null;
    }
    const handle = Mojo.createSharedBuffer(data.byteLength).handle;
    const buffer = new Uint8Array(handle.mapBuffer(0, data.byteLength).buffer);
    buffer.set(data);

    // BigBuffer type wants all properties but Mojo expects only one of them.
    const bigBuffer = {
      sharedMemory: {
        bufferHandle: handle,
        size: data.byteLength,
      },
      bytes: undefined,
      invalidBuffer: undefined,
    };
    delete bigBuffer.invalidBuffer;
    delete bigBuffer.bytes;
    const {bitmap} = await this.proxy_.handler.decodeBitmap(bigBuffer);
    return bitmap;
  }
  private async decodeAudio_(): Promise<AudioData> {
    const audioCtx = new AudioContext({sampleRate: 48000});
    const arrayBuffer = await this.audioFile_!.arrayBuffer();
    const buffer = await audioCtx.decodeAudioData(arrayBuffer);
    if (buffer.numberOfChannels > 1) {
      throw new Error('Multichannel audio is not supported');
    }
    return {
      sampleRate: buffer.sampleRate,
      channelCount: buffer.numberOfChannels,
      frameCount: buffer.length,
      data: Array.from(buffer.getChannelData(0)),
    };
  }
  private async onExecute_() {
    this.imageError_ = '';
    if (this.session_ === null) {
      return;
    }
    if (!this.$.topKInput.validate()) {
      return;
    }
    if (!this.$.temperatureInput.validate()) {
      return;
    }
    const pieces = textToInputPieces(this.text_);
    if (this.imageFile_ !== null) {
      const bitmap = await this.decodeBitmap_();
      if (bitmap) {
        pieces.unshift({bitmap});
      } else {
        this.imageFile_ = null;
        this.imageError_ = 'Image is invalid';
        return;
      }
    }
    if (this.audioFile_ !== null) {
      try {
        const audio = await this.decodeAudio_();
        pieces.unshift({audio});
      } catch (error) {
        this.audioFile_ = null;
        this.audioError_ = `Audio is invalid: ${error}`;
        return;
      }
    }
    const clonedSession = new SessionRemote();
    this.session_.clone(clonedSession.$.bindNewPipeAndPassReceiver());
    clonedSession.append(
        {
          maxTokens: 0,
          input: {pieces: pieces},
        },
        null);
    clonedSession.generate(
        {
          maxOutputTokens: 0,
          constraint: null,
        },
        this.responseRouter_.$.bindNewPipeAndPassRemote());
    const onResponseId =
        this.responseRouter_.onResponse.addListener((chunk: ResponseChunk) => {
          assert(this.currentResponse_);
          this.currentResponse_.response =
              (this.currentResponse_?.response + chunk.text).trimStart();
          this.requestUpdate();
        });
    const onCompleteId =
        this.responseRouter_.onComplete.addListener((_: ResponseSummary) => {
          this.addResponse_();
          this.responseRouter_.removeListener(onResponseId);
          this.responseRouter_.removeListener(onCompleteId);
        });
    this.currentResponse_ = {
      text: this.text_,
      response: '',
      responseClass: 'response',
      retracted: false,
      error: false,
    };
    this.text_ = '';
  }

  protected canEnterInput_(): boolean {
    return !this.currentResponse_ && this.model_ !== null;
  }

  protected canExecute_(): boolean {
    return this.canEnterInput_() && this.text_.length > 0;
  }

  protected canUploadFile_(): boolean {
    return this.canEnterInput_() && this.imageFile_ === null;
  }

  protected isLoading_(): boolean {
    return this.loadModelStart_ !== 0;
  }

  protected imagesEnabled_(): boolean {
    return this.capabilities_.imageInput;
  }

  protected audioEnabled_(): boolean {
    return this.capabilities_.audioInput;
  }

  protected getModelText_(): string {
    if (this.modelPath_.length === 0) {
      return '';
    }
    let text = 'Model loaded from ' + this.modelPath_ + ' in ' +
        this.loadModelDuration_ + 'ms ';
    if (this.imagesEnabled_()) {
      text += '[images enabled]';
    }
    if (this.audioEnabled_()) {
      text += '[audio enabled]';
    }
    if (this.loadedPerformanceHint_ ===
        ModelPerformanceHint.kFastestInference) {
      text += '[fastest inference]';
    }
    return text;
  }

  protected onContextExpandedChanged_(e: CustomEvent<{value: boolean}>) {
    this.contextExpanded_ = e.detail.value;
  }

  protected onContextTextChanged_(e: CustomEvent<{value: string}>) {
    this.contextText_ = e.detail.value;
  }

  protected onTextChanged_(e: CustomEvent<{value: string}>) {
    this.text_ = e.detail.value;
  }

  protected onTopKChanged_(e: CustomEvent<{value: number}>) {
    this.topK_ = e.detail.value;
  }

  protected onTemperatureChanged_(e: CustomEvent<{value: number}>) {
    this.temperature_ = e.detail.value;
  }
}

export type ToolsElement = OnDeviceInternalsToolsElement;

declare global {
  interface HTMLElementTagNameMap {
    'on-device-internals-tools': OnDeviceInternalsToolsElement;
  }
}

customElements.define(
    OnDeviceInternalsToolsElement.is, OnDeviceInternalsToolsElement);
