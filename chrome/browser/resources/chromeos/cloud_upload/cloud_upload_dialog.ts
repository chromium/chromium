// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getTemplate} from './cloud_upload_dialog.html.js';

/**
 * @fileoverview
 * 'cloud-upload-dialog' defines the UI for the "Upload to cloud" workflow.
 */

class CloudUploadDialogElement extends HTMLElement {
  constructor() {
    super();
    const template = document.createElement('template');
    template.innerHTML = getTemplate() as string;
    const fragment = template.content.cloneNode(true);
    this.attachShadow({mode: 'open'}).appendChild(fragment);
  }
}

customElements.define('cloud-upload-dialog', CloudUploadDialogElement);
