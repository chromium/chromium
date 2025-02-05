// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * List item that displays an APN a user may select to attempt to be used.
 */

import '//resources/ash/common/cr_elements/cr_shared_style.css.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';

import type {ApnProperties} from '//resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {I18nMixin, I18nMixinInterface} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';

import {getTemplate} from './apn_selection_dialog_list_item.html.js';
import {getApnDisplayName} from './cellular_utils.js';

const ApnSelectionDialogListItemElementBase = I18nMixin(PolymerElement);

export class ApnSelectionDialogListItem extends
    ApnSelectionDialogListItemElementBase {
  static get is() {
    return 'apn-selection-dialog-list-item' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      apn: {
        type: Object,
      },

      selected: {
        type: Boolean,
        reflectToAttribute: true,
      },
    };
  }

  apn: ApnProperties;
  selected: boolean;

  private getApnDisplayName_(apn: ApnProperties): string {
    return getApnDisplayName(this.i18n.bind(this), apn);
  }

  private shouldHideSecondaryApnName_(apn: ApnProperties): boolean {
    return apn.accessPointName === this.getApnDisplayName_(apn);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [ApnSelectionDialogListItem.is]: ApnSelectionDialogListItem;
  }
}

customElements.define(
    ApnSelectionDialogListItem.is, ApnSelectionDialogListItem);
