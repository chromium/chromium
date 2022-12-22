// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ManagedCellularProperties} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';

export class ApnList extends PolymerElement {
  static get is(): string;
  static get template(): HTMLTemplateElement;
  static get properties(): {
    guid: StringConstructor,
    managedCellularProperties: ManagedCellularProperties,
    shouldOmitLinks: {
      type: BooleanConstructor,
      value: boolean,
    },
    shouldShowApnDetailDialog_: {
      type: BooleanConstructor,
      value: boolean,
    },
    isConnectedApnAutoDetected_: {
      type: BooleanConstructor,
      value: boolean,
    },
  };
  openApnDetailDialogInCreateMode(): void;
  private getApns_;
  private isConnectedApnAutoDetected_: boolean;
  private isApnConnected_;
  private isApnAutoDetected_;
  private onLearnMoreClicked_;
  private onShowApnDetailDialog_;
  private showApnDetailDialog_;
  private shouldShowApnDetailDialog_: boolean;
  private onApnDetailDialogClose_;
}

declare global {
  interface HTMLElementTagNameMap {
    'apn-list': ApnList;
  }
}
