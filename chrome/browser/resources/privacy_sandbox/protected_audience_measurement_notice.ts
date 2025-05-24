// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {BaseDialogMixin} from './base_dialog_mixin.js';
import {PrivacySandboxNotice} from './notice.mojom-webui.js';
import {getHtml} from './protected_audience_measurement_notice.html.js';

const ProtectedAudienceMeasurementNoticeBase = BaseDialogMixin(
    CrLitElement, PrivacySandboxNotice.kProtectedAudienceMeasurementNotice);

export class ProtectedAudienceMeasurementNotice extends
    ProtectedAudienceMeasurementNoticeBase {
  static get is() {
    return 'protected-audience-measurement-notice';
  }

  override render() {
    return getHtml.bind(this)();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'protected-audience-measurement-notice': ProtectedAudienceMeasurementNotice;
  }
}

customElements.define(
    ProtectedAudienceMeasurementNotice.is, ProtectedAudienceMeasurementNotice);
