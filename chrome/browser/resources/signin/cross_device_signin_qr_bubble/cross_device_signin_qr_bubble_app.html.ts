// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CrossDeviceSigninQrBubbleAppElement} from './cross_device_signin_qr_bubble_app.js';

export function getHtml(this: CrossDeviceSigninQrBubbleAppElement) {
  return html`
<div id="bubble-container">
  <p id="subtitle">${this.i18n('subtitle')}</p>

  <div id="card-container">
    <div id="qr-wrapper">
      ${
      this.qrCodeDataUri ? html`
        <img id="qr-code" src="${this.qrCodeDataUri}" alt="QR Code">
      ` :
                           ''}
    </div>

    <div id="user-details">
      <div id="full-name">${this.fullName}</div>
      <div id="email">${this.email}</div>
    </div>
  </div>
</div>
`;
}
