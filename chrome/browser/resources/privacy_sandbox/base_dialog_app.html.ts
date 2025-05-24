// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {BaseDialogApp} from './base_dialog_app.js';
import {PrivacySandboxNotice} from './notice.mojom-webui.js';

export function getHtml(this: BaseDialogApp) {
  return html`
    <cr-view-manager id="viewManager">
      <topics-consent-notice id="${
      this.getNoticeId(PrivacySandboxNotice.kTopicsConsentNotice)}"
          slot="view"
          fill-content>
      </topics-consent-notice>
      <protected-audience-measurement-notice id="${
      this.getNoticeId(
          PrivacySandboxNotice.kProtectedAudienceMeasurementNotice)}"
          slot="view"
          fill-content>
      </protected-audience-measurement-notice>
      <three-ads-apis-notice id="${
      this.getNoticeId(
          PrivacySandboxNotice.kThreeAdsApisNotice)}" slot="view" fill-content>
      </three-ads-apis-notice>
      <measurement-notice id="${
      this.getNoticeId(
          PrivacySandboxNotice.kMeasurementNotice)}" slot="view" fill-content>
      </measurement-notice>
    </cr-view-manager>
  `;
}
