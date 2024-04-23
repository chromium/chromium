// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ManagedCellularProperties} from '//resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {PortalState} from '//resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

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
    shouldDisallowApnModification: {
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
    shouldShowApnSelectionDialog_: {
      type: BooleanConstructor,
      value: boolean,
    },
  };
  errorState: string;
  portalState: PortalState;
  shouldOmitLinks: boolean;
  shouldDisallowApnModification: boolean;
  openApnDetailDialogInCreateMode(): void;
  openApnSelectionDialog(): void;
  private getApns_;
  private isConnectedApnAutoDetected_: boolean;
  private isApnConnected_;
  private isApnAutoDetected_;
  private onLearnMoreClicked_;
  private onShowApnDetailDialog_;
  private showApnDetailDialog_;
  private shouldShowApnDetailDialog_: boolean;
  private onApnDetailDialogClose_;
  private shouldShowApnSelectionDialog_: boolean;
  private onApnSelectionDialogClose_;
}

declare global {
  interface HTMLElementTagNameMap {
    'apn-list': ApnList;
  }
}
