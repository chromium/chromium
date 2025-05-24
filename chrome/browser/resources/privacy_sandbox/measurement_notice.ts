// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {BaseDialogMixin} from './base_dialog_mixin.js';
import {getHtml} from './measurement_notice.html.js';
import {PrivacySandboxNotice} from './notice.mojom-webui.js';

const MeasurementNoticeBase =
    BaseDialogMixin(CrLitElement, PrivacySandboxNotice.kMeasurementNotice);

export class MeasurementNotice extends MeasurementNoticeBase {
  static get is() {
    return 'measurement-notice';
  }

  override render() {
    return getHtml.bind(this)();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'measurement-notice': MeasurementNotice;
  }
}

customElements.define(MeasurementNotice.is, MeasurementNotice);
