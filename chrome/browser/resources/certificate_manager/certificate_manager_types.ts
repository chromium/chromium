// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CertificateProvisioningProcess} from './certificate_provisioning_browser_proxy.js';

/**
 * The payload of the 'certificate-provisioning-view-details-action' event.
 */
export interface CertificateProvisioningActionEventDetail {
  model: CertificateProvisioningProcess;
  anchor: HTMLElement;
}

/**
 * The name of the event fired when a the "View Details" action is selected on
 * the dropdown menu next to a certificate provisioning process.
 * CertificateActionEventDetail is passed as the event detail.
 */
export const CertificateProvisioningViewDetailsActionEvent =
    'certificate-provisioning-view-details-action';

declare global {
  interface HTMLElementEventMap {
    'certificate-provisioning-view-details-action':
        CustomEvent<CertificateProvisioningActionEventDetail>;
  }
}
