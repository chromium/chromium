// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {I18nMixin} from '//resources/ash/common/cr_elements/i18n_mixin.js';
import {ApnProperties} from '//resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

export class ApnSelectionDialog extends I18nMixin
(PolymerElement) {
apnList:
  ApnProperties[];
guid:
  string;
}

declare global {
  interface HTMLElementTagNameMap {
    'apn-selection-dialog': ApnSelectionDialog;
  }
}
