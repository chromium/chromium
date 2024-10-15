// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {I18nMixin} from '//resources/ash/common/cr_elements/i18n_mixin.js';
import {ApnProperties} from '//resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {PortalState} from '//resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

export class ApnListItem extends I18nMixin
(PolymerElement) {
guid:
  string;
apn:
  ApnProperties;
isConnected:
  boolean;
shouldDisallowDisablingRemoving:
  boolean;
shouldDisallowEnabling:
  boolean;
shouldDisallowApnModification:
  boolean;
itemIndex:
  number;
listSize:
  number;
portalState:
  PortalState|null;
private isDisabled_:
  boolean;
private isApnRevampAndAllowApnModificationPolicyEnabled_:
  boolean;
}

declare global {
  interface HTMLElementTagNameMap {
    'apn-list-item': ApnListItem;
  }
}
