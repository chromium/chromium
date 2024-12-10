// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import '//resources/cr_elements/cr_collapse/cr_collapse.js';
import '//resources/cr_elements/cr_expand_button/cr_expand_button.js';
import '//resources/cr_elements/cr_hidden_style.css.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/cr_elements/cr_shared_vars.css.js';
import '//resources/cr_elements/cr_textarea/cr_textarea.js';

import type {CrInputElement} from '//resources/cr_elements/cr_input/cr_input.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxy} from './browser_proxy.js';
import type {InputPiece, ResponseChunk, ResponseSummary} from './on_device_model.mojom-webui.js';
import {LoadModelResult, OnDeviceModelRemote, PerformanceClass, SessionRemote, StreamingResponderCallbackRouter, Token} from './on_device_model.mojom-webui.js';
import {getTemplate} from './tools.html.js';

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
    topKInput: CrInputElement,
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
        input.length === 0 || input[input.length - 1].text === undefined) {
      input.push({text: piece});
    } else {
      input[input.length - 1].text += '\n' + piece;
    }
  }
  return input;
}

class OnDeviceInternalsToolsElement extends PolymerElement {
  static get is() {
    return 'on-device-internals-tools';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      modelPath_: {
        type: String,
        value: '',
      },
      error_: String,
      imageError_: String,
      text_: String,
      loadModelStart_: {
        type: Number,
        value: 0,
      },
      currentResponse_: {
        type: Object,
        value: null,
      },
      responses_: {
        type: Array,
        value: () => [],
      },
      baseModel_: {
        type: Object,
        value: null,
      },
      model_: {
        type: Object,
        value: null,
      },
      performanceClassText_: {
        type: String,
        value: 'Loading...',
      },
      contextExpanded_: Boolean,
      contextLength_: Number,
      contextText_: String,
      enableImageInput_: Boolean,
      topK_: Number,
      temperature_: Number,
      imageFile_: {
        type: Object,
        value: null,
      },
    };
  }

  static get observers() {
    return [
      'onModelOrErrorChanged_(model_, error_)',
    ];
  }

  private contextExpanded_: boolean = false;
  private contextLength_: number = 0;
  private contextText_: string;
  private currentResponse_: Response|null;
  private error_: string;
  private imageError_: string;
  private loadModelDuration_: number;
  private loadModelStart_: number;
  private modelPath_: string;
  private baseModel_: OnDeviceModelRemote|null;
  private model_: OnDeviceModelRemote|null;
  private performanceClassText_: string;
  private responses_: Response[];
  private session_: SessionRemote|null = null;
  private temperature_: number = 0;
  private text_: string;
  private topK_: number = 1;
  private imageFile_: File|null = null;
  private enableImageInput_: boolean = false;

  private proxy_: BrowserProxy = BrowserProxy.getInstance();
  private responseRouter_: StreamingResponderCallbackRouter =
      new StreamingResponderCallbackRouter();

  override ready() {
    super.ready();
    this.getPerformanceClass_();
    this.$.temperatureInput.inputElement.step = '0.1';
    this.$.imageInput.addEventListener(
        'change', this.onImageChange_.bind(this));
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

  private onLoadClick_() {
    this.onModelSelected_();
  }

  private onAddImageClick_() {
    this.$.imageInput.click();
  }

  private onRemoteImageClick_() {
    this.imageFile_ = null;
    this.$.imageInput.value = '';
  }

  private onServiceCrashed_() {
    if (this.currentResponse_) {
      this.currentResponse_.error = true;
      this.addResponse_();
    }
    this.error_ = 'Service crashed, please reload the model.';
    this.model_ = null;
    this.baseModel_ = null;
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

  private async onModelSelected_() {
    this.error_ = '';
    if (this.baseModel_) {
      this.baseModel_.$.close();
    }
    if (this.model_) {
      this.model_.$.close();
    }
    this.imageFile_ = null;
    this.baseModel_ = null;
    this.model_ = null;
    this.loadModelStart_ = new Date().getTime();
    const modelPath = this.$.modelInput.value;
    // <if expr="is_win">
    // Windows file paths are std::wstring, so use Array<Number>.
    const processedPath = Array.from(modelPath, (c) => c.charCodeAt(0));
    // </if>
    // <if expr="not is_win">
    const processedPath = modelPath;
    // </if>
    const baseModel = new OnDeviceModelRemote();
    let newModel = new OnDeviceModelRemote();
    let {result} = await this.proxy_.handler.loadModel(
        {path: processedPath}, baseModel.$.bindNewPipeAndPassReceiver());
    if (result === LoadModelResult.kSuccess && this.enableImageInput_) {
      result = (await baseModel.loadAdaptation(
                    {
                      enableImageInput: true,
                      maxTokens: 0,
                      assets: {
                        weights: null,
                        weightsPath: null,
                      },
                    },
                    newModel.$.bindNewPipeAndPassReceiver()))
                   .result;
    } else {
      // No adaptation needed, just use the base model.
      newModel = baseModel;
    }
    if (result !== LoadModelResult.kSuccess) {
      this.error_ =
          'Unable to load model. Specify a correct and absolute path.';
    } else {
      this.baseModel_ = baseModel;
      this.model_ = newModel;
      this.model_.onConnectionError.addListener(() => {
        this.onServiceCrashed_();
      });
      this.startNewSession_();
      this.modelPath_ = modelPath;
    }
  }

  private onAddContextClick_() {
    if (this.session_ === null) {
      return;
    }
    this.session_.addContext(
        {
          ignoreContext: false,
          maxTokens: 0,
          tokenOffset: 0,
          maxOutputTokens: 0,
          topK: null,
          temperature: null,
          input: {pieces: textToInputPieces(this.contextText_)},
        },
        null);
    this.contextLength_ += this.contextText_.split(/(\s+)/).length;
    this.contextText_ = '';
  }

  private startNewSession_() {
    if (this.model_ === null) {
      return;
    }
    this.contextLength_ = 0;
    this.session_ = new SessionRemote();
    this.model_.startSession(this.session_.$.bindNewPipeAndPassReceiver());
  }

  private onCancelClick_() {
    this.responseRouter_.$.close();
    this.responseRouter_ = new StreamingResponderCallbackRouter();
    this.addResponse_();
  }

  private async onExecuteClick_() {
    await this.onExecute_();
  }

  private addResponse_() {
    this.unshift('responses_', this.currentResponse_);
    this.currentResponse_ = null;
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
    this.session_.execute(
        {
          ignoreContext: false,
          maxTokens: 0,
          tokenOffset: 0,
          maxOutputTokens: 0,
          topK: this.topK_,
          temperature: this.temperature_,
          input: {pieces: pieces},
        },
        this.responseRouter_.$.bindNewPipeAndPassRemote());
    const onResponseId =
        this.responseRouter_.onResponse.addListener((chunk: ResponseChunk) => {
          this.set(
              'currentResponse_.response',
              (this.currentResponse_?.response + chunk.text).trimStart());
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

  private canExecute_(): boolean {
    return !this.currentResponse_ && this.model_ !== null;
  }

  private canUploadFile_(): boolean {
    return this.canExecute_() && this.imageFile_ === null;
  }

  private isLoading_(): boolean {
    return this.loadModelStart_ !== 0;
  }

  private imagesEnabled_(): boolean {
    return this.model_ !== this.baseModel_;
  }

  private getModelText_(): string {
    if (this.modelPath_.length === 0) {
      return '';
    }
    let text = 'Model loaded from ' + this.modelPath_ + ' in ' +
        this.loadModelDuration_ + 'ms';
    if (this.imagesEnabled_()) {
      text += ' [images enabled]';
    }
    return text;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'on-device-internals-tools': OnDeviceInternalsToolsElement;
  }
}

customElements.define(
    OnDeviceInternalsToolsElement.is, OnDeviceInternalsToolsElement);
