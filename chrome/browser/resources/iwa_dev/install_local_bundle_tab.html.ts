// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {IwaDevInstallLocalBundleTabElement} from './install_local_bundle_tab.js';

export function getHtml(this: IwaDevInstallLocalBundleTabElement) {
  return html`
<div
    class="dropzone ${this.isDragOver_ ? 'drag-over' : ''} ${
        this.selectedFileName_ ? 'has-file' : ''}"
    role="button" tabindex="0" title="${this.selectedFileName_ || nothing}"
    aria-label="${this.selectedFileName_ ?
        `Selected bundle: ${
            this.selectedFileName_}. Drop another file or click to replace.` :
        'Drag and drop .swbn file here, or click to browse'}"
    @click="${this.onDropzoneClick_}" @keydown="${this.onDropzoneKeydown_}"
    @dragover="${this.onDragover_}" @dragleave="${this.onDragleave_}"
    @drop="${this.onDrop_}">
  <cr-icon icon="cr:insert-drive-file"></cr-icon>
  ${this.selectedFileName_ ? html`
    <div class="dropzone-filename">
      ${this.selectedFileName_}
    </div>
    <div class="dropzone-subtext">
      Drop another file or click to replace
    </div>
  ` : html`
    <div class="dropzone-prompt">
      Drag & drop .swbn file here, or click to browse
    </div>
    <div class="dropzone-subtext">
      Supports signed web bundle (.swbn)
    </div>
  `}
</div>
${this.errorMessage_ ? html`
  <div class="error-message" aria-live="polite">
    ${this.errorMessage_}
  </div>
` : ''}`;
}
