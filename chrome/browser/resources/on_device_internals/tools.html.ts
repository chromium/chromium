// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ToolsElement} from './tools.js';

export function getHtml(this: ToolsElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="performance-class">
  Device performance class: <strong>${this.performanceClassText_}</strong>
</div>
<cr-input id="modelInput" label="Model directory" placeholder="/tmp/model"
    ?disabled="${this.isLoading_()}"
    error-message="${this.error_}" ?invalid="${this.error_.length}" autofocus>
  <cr-button slot="suffix" ?disabled="${this.isLoading_()}"
      @click="${this.onLoadClick_}">
    Load
  </cr-button>
</cr-input>
<div class="model-options">
  <select id="performanceHintSelect" class="md-select"
      value="${this.performanceHint_}" @change="${this.onPerformanceHintChange_}">
    <option value="kHighestQuality">Highest Quality</option>
    <option value="kFastestInference">Fastest Inference</option>
  </select>
</div>
<div class="model-text">
  ${this.getModelText_()}
  <div class="throbber" ?hidden="${!this.isLoading_()}"></div>
</div>

<cr-expand-button class="cr-row first" ?expanded="${this.contextExpanded_}"
    @expanded-changed="${this.onContextExpandedChanged_}"
    ?disabled="${!this.model_}">
  Context options (current size: ${this.contextLength_} words)
</cr-expand-button>
<cr-collapse id="expandedContent" ?opened="${this.contextExpanded_}">
  <cr-textarea type="text" label="Context" .value="${this.contextText_}"
      @value-changed="${this.onContextTextChanged_}">
  </cr-textarea>
  <cr-button @click="${this.onAddContextClick_}">Add</cr-button>
  <cr-button class="cr-button-gap" @click="${this.startNewSession_}">
    New session
  </cr-button>
</cr-collapse>
<div class="output-options">
  <cr-input id="topKInput" type="number" min="1" max="128" label="Top K"
      error-message="Top K must be between 1 and 128" auto-validate
      .value="${this.topK_}" @value-changed="${this.onTopKChanged_}">
  </cr-input>
  <cr-input id="temperatureInput" type="number" min="0" max="2"
      label="Temperature" auto-validate
      error-message="Temperature must be between 0 and 2"
      .value="${this.temperature_}"
      @value-changed="${this.onTemperatureChanged_}">
  </cr-input>
</div>
<cr-textarea type="text" id="textInput" label="Input"
    placeholder="Place control tokens {$SYSTEM, $MODEL, $USER, $END} on their own lines, in between lines of text."
    .value="${this.text_}" @value-changed="${this.onTextChanged_}">
</cr-textarea>
<div class="multimodal-buttons" >
  <div class="image-buttons" ?hidden="${!this.imagesEnabled_()}">
    <div class="image-error">${this.imageError_}</div>
    <div ?hidden="${this.imageFile_}">
      <cr-button class="floating-button"
          ?disabled="${!this.canUploadFile_()}"
          @click="${this.onAddImageClick_}">
        <cr-icon icon="cr:add" slot="prefix-icon"></cr-icon>
        Add image
      </cr-button>
      <input id="imageInput" type="file">
    </div>
    ${this.imageFile_ ? html`
      <cr-button class="floating-button" @click="${this.onRemoteImageClick_}">
        <cr-icon icon="cr:delete" slot="prefix-icon"></cr-icon>
        ${this.imageFile_.name}
      </cr-button>
    ` : ''}
  </div>
  <div class="audio-buttons" ?hidden="${!this.audioEnabled_()}">
    <div class="audio-error">${this.audioError_}</div>
    <div ?hidden="${this.audioFile_}">
      <cr-button class="floating-button"
          ?disabled="${!this.canUploadFile_()}"
          @click="${this.onAddAudioClick_}">
        <cr-icon icon="cr:add" slot="prefix-icon"></cr-icon>
        Add audio
      </cr-button>
      <input id="audioInput" type="file" accept="audio/*">
    </div>
    ${this.audioFile_ ? html`
      <cr-button class="floating-button" @click="${this.onRemoteAudioClick_}">
        <cr-icon icon="cr:delete" slot="prefix-icon"></cr-icon>
        ${this.audioFile_.name}
      </cr-button>
    ` : ''}
  </div>
</div>

<div>
  <cr-button class="action-button" ?disabled="${!this.canExecute_()}"
      @click="${this.onExecuteClick_}">
    Execute
  </cr-button>
  <cr-button ?disabled="${!this.currentResponse_}"
      @click="${this.onCancelClick_}">
    Cancel
  </cr-button>
</div>

${this.currentResponse_ ? html`
  <div class="session">
    <div class="text">${this.currentResponse_.text}</div>
    <div ?hidden="${!this.currentResponse_.response.length}"
        class="${this.currentResponse_.responseClass}"><!--
        -->${this.currentResponse_.response}</div>
    <div class="throbber"
        ?hidden="${this.currentResponse_.response.length}"></div>
  </div>
` : ''}
${this.responses_.map(item => html`
  <div class="session">
    <div class="text">${item.text}</div>
    <div class="${item.responseClass}">${item.response}</div>
  </div>
`)}
<!--_html_template_end_-->`;
  // clang-format on
}
