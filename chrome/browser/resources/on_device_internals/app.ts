// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_hidden_style.css.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/cr_elements/cr_shared_vars.css.js';

import {CrInputElement} from '//resources/cr_elements/cr_input/cr_input.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';
import {BrowserProxy} from './browser_proxy.js';
import {OnDeviceModelRemote, PerformanceClass, StreamingResponderCallbackRouter} from './on_device_model.mojom-webui.js';

interface Response {
  text: string;
  response: string;
}

interface OnDeviceInternalsAppElement {
  $: {
    modelInput: CrInputElement,
    textInput: CrInputElement,
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
    default:
      return 'Error';
  }
}

class OnDeviceInternalsAppElement extends PolymerElement {
  static get is() {
    return 'on-device-internals-app';
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
      model_: {
        type: Object,
        value: null,
      },
      performanceClassText_: {
        type: String,
        value: 'Loading...',
      },
    };
  }

  static get observers() {
    return [
      'onModelOrErrorChanged_(model_, error_)',
    ];
  }

  private currentResponse_: Response|null;
  private error_: string;
  private loadModelDuration_: number;
  private loadModelStart_: number;
  private modelPath_: string;
  private model_: OnDeviceModelRemote|null;
  private performanceClassText_: string;
  private responses_: Response[];
  private text_: string;

  private proxy_: BrowserProxy = BrowserProxy.getInstance();

  override ready() {
    super.ready();
    this.getPerformanceClass_();
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

  private async onModelSelected_() {
    this.error_ = '';
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
    const {result} = await this.proxy_.handler.loadModel({path: processedPath});
    if (result.error) {
      this.error_ = result.error;
    } else {
      this.model_ = result.model || null;
      this.modelPath_ = modelPath;
    }
  }

  private onExecuteClick_() {
    this.onExecute_();
  }

  private onExecute_() {
    if (this.model_ === null) {
      return;
    }
    const router = new StreamingResponderCallbackRouter();
    this.model_.execute(this.text_, router.$.bindNewPipeAndPassRemote());
    const onResponseId = router.onResponse.addListener((text: string) => {
      this.set(
          'currentResponse_.response',
          (this.currentResponse_?.response + text).trimStart());
    });
    const onCompleteId = router.onComplete.addListener(() => {
      this.unshift('responses_', this.currentResponse_);
      this.currentResponse_ = null;
      this.$.textInput.focus();

      router.removeListener(onResponseId);
      router.removeListener(onCompleteId);
    });
    this.currentResponse_ = {text: this.text_, response: ''};
    this.text_ = '';
  }

  private canExecute_(): boolean {
    return !this.currentResponse_ && this.model_ !== null;
  }

  private isLoading_(): boolean {
    return this.loadModelStart_ !== 0;
  }

  private getModelText_(): string {
    if (this.modelPath_.length === 0) {
      return '';
    }
    return 'Model loaded from ' + this.modelPath_ + ' in ' +
        this.loadModelDuration_ + 'ms';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'on-device-internals-app': OnDeviceInternalsAppElement;
  }
}

customElements.define(
    OnDeviceInternalsAppElement.is, OnDeviceInternalsAppElement);
