// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './settings_section.css.js';
import {getHtml} from './settings_section.html.js';

export class PrintPreviewSettingsSectionElement extends CrLitElement {
  static get is() {
    return 'print-preview-settings-section';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }
}

export type SettingsSectionElement = PrintPreviewSettingsSectionElement;

customElements.define(
    PrintPreviewSettingsSectionElement.is, PrintPreviewSettingsSectionElement);
