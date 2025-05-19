// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {BaseDialogApp} from './base_dialog_app.js';
import {PrivacySandboxNotice} from './notice.mojom-webui.js';

export function getHtml(this: BaseDialogApp) {
  return html`
    <cr-view-manager id="viewManager">
      <topics-consent id="${
      this.getNoticeId(PrivacySandboxNotice.kTopicsConsentNotice)}"
          slot="view"
          fill-content>
      </topics-consent>
      <protected-audience-measurement id="${
      this.getNoticeId(
          PrivacySandboxNotice.kProtectedAudienceMeasurementNotice)}"
          slot="view"
          fill-content>
      </protected-audience-measurement>
    </cr-view-manager>
  `;
}
