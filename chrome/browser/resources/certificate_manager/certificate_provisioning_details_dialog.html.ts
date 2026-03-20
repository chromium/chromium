// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CertificateProvisioningDetailsDialogElement} from './certificate_provisioning_details_dialog.js';

export function getHtml(this: CertificateProvisioningDetailsDialogElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<cr-dialog id="dialog" show-on-attach show-close-button
    close-text="$i18n{close}">
  <div slot="title">
    $i18n{certificateProvisioningDetails}
  </div>
  <div slot="body">
    <div class="two-line">
      <div class="label" aria-describedby="certProfileName">
        $i18n{certificateProvisioningProfileName}
      </div>
      <div class="value" id="certProfileName" aria-hidden="true">
        ${this.model.certProfileName}
      </div>
    </div>
    <div class="two-line">
      <div class="label" aria-describedby="certProfileId">
        $i18n{certificateProvisioningProfileId}
      </div>
      <div class="value" id="certProfileId" aria-hidden="true">
        ${this.model.certProfileId}
      </div>
    </div>
    <div class="two-line">
      <div class="label" aria-describedby="processId">
        $i18n{certificateProvisioningProcessId}
      </div>
      <div class="value" id="processId" aria-hidden="true">
        ${this.model.processId}
      </div>
    </div>
    <div class="button-box">
      <div class="two-line flex">
        <div class="label" aria-describedby="status">
          $i18n{certificateProvisioningStatus}
        </div>
        <span class="value" id="status" aria-hidden="true">
          ${this.model.status}
        </span>
      </div>
      <cr-button id="refresh" role="button" @click="${this.onRefreshClick_}">
        $i18n{certificateProvisioningRefresh}
      </cr-button>
    </div>
    <div class="two-line">
      <div class="label" aria-describedby="timeSinceLastUpdate">
        $i18n{certificateProvisioningLastUpdate}
      </div>
      <div class="value" id="timeSinceLastUpdate" aria-hidden="true">
        ${this.model.timeSinceLastUpdate}
      </div>
    </div>
    <div class="two-line"
        ?hidden="${this.shouldHideLastFailedStatus_()}">
      <div class="label" aria-describedby="lastFailedStatus">
        $i18n{certificateProvisioningLastUnsuccessfulStatus}
      </div>
      <div class="value" id="lastFailedStatus" aria-hidden="true">
        ${this.model.lastUnsuccessfulMessage}
      </div>
    </div>
    <cr-button id="reset" role="button" @click="${this.onResetClick_}">
      $i18n{certificateProvisioningReset}
    </cr-button>
    <hr>
    <cr-expand-button .expanded="${this.advancedExpanded_}"
        @expanded-changed="${this.onAdvancedExpandedChanged_}">
      <div>$i18n{certificateProvisioningAdvancedSectionTitle}</div>
    </cr-expand-button>
    <cr-collapse id="advancedInfo" ?opened="${this.advancedExpanded_}">
      <div class="two-line">
        <div class="label" aria-describedby="stateId">
          $i18n{certificateProvisioningStatusId}
        </div>
        <div class="value" id="stateId" aria-hidden="true">
          ${this.model.stateId}
        </div>
      </div>
      <div class="two-line">
        <div class="label" aria-describedby="publicKey">
          $i18n{certificateProvisioningPublicKey}
        </div>
        <div class="value" id="publicKey" aria-hidden="true">
          ${this.model.publicKey}
        </div>
      </div>
    </cr-collapse>
  </div>
</cr-dialog>
<!--_html_template_end_-->`;
  // clang-format on
}
