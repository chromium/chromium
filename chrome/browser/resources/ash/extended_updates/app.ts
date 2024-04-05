// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/ash/common/cr_elements/localized_link/localized_link.js';
import 'chrome://resources/cros_components/button/button.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';
import {ExtendedUpdatesBrowserProxy} from './extended_updates_browser_proxy.js';

export class ExtendedUpdatesAppElement extends PolymerElement {
  static get is() {
    return 'extended-updates-app' as const;
  }

  static get template() {
    return getTemplate();
  }

  // Shows the confirmation popup when true.
  private showPopup_: boolean;

  private browserProxy_: ExtendedUpdatesBrowserProxy;

  constructor() {
    super();

    this.browserProxy_ = ExtendedUpdatesBrowserProxy.getInstance();
  }

  private onEnableButtonClick_(): void {
    this.showPopup_ = true;
  }

  private onCancelButtonClick_(): void {
    this.browserProxy_.closeDialog();
  }

  private onPopupConfirmButtonClick_(): void {
    this.browserProxy_.optInToExtendedUpdates();
    this.browserProxy_.closeDialog();
  }

  private onPopupCancelButtonClick_(): void {
    this.showPopup_ = false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [ExtendedUpdatesAppElement.is]: ExtendedUpdatesAppElement;
  }
}

customElements.define(ExtendedUpdatesAppElement.is, ExtendedUpdatesAppElement);
