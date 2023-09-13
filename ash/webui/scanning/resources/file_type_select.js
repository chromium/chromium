// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './scan_settings_section.js';
import './strings.m.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview
 * 'file-type-select' displays the available file types in a dropdown.
 */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const FileTypeSelectElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
class FileTypeSelectElement extends FileTypeSelectElementBase {
  static get is() {
    return 'file-type-select';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {boolean} */
      disabled: Boolean,

      /** @type {string} */
      selectedFileType: {
        type: String,
        notify: true,
      },
    };
  }
}

customElements.define(FileTypeSelectElement.is, FileTypeSelectElement);
