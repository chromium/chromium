// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './scan_settings_section.js';
import './strings.m.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './file_type_select.html.js';

/**
 * @fileoverview
 * 'file-type-select' displays the available file types in a dropdown.
 */

const FileTypeSelectElementBase = I18nMixin(PolymerElement);

export class FileTypeSelectElement extends FileTypeSelectElementBase {
  static get is() {
    return 'file-type-select' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      disabled: Boolean,

      selectedFileType: {
        type: String,
        notify: true,
      },
    };
  }

  disabled: boolean;
  selectedFileType: string;
}

declare global {
  interface HTMLElementTagNameMap {
    [FileTypeSelectElement.is]: FileTypeSelectElement;
  }
}

customElements.define(FileTypeSelectElement.is, FileTypeSelectElement);
