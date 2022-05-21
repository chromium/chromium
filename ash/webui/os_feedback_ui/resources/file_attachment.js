// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './help_resources_icons.js';
import './os_feedback_shared_css.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview
 * 'file-attachment' allows users to select a file as an attachment to the
 *  report.
 */
export class FileAttachmentElement extends PolymerElement {
  static get is() {
    return 'file-attachment';
  }

  static get template() {
    return html`{__html_template__}`;
  }
}

customElements.define(FileAttachmentElement.is, FileAttachmentElement);
