// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-webui.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
import './firmware_shared.css.js';
import './firmware_shared_fonts.css.js';
import './firmware_update.mojom-webui.js';
import './strings.m.js';

import {I18nMixin, I18nMixinInterface} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {mojoString16ToString} from 'chrome://resources/js/mojo_type_util.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {FirmwareUpdate, UpdatePriority} from './firmware_update.mojom-webui.js';
import {OpenConfirmationDialogEventDetail} from './firmware_update_types.js';
import {getTemplate} from './update_card.html.js';

/**
 * @fileoverview
 * 'update-card' displays information about a peripheral update.
 */

const UpdateCardElementBase =
    I18nMixin(PolymerElement) as {new (): PolymerElement & I18nMixinInterface};

export class UpdateCardElement extends UpdateCardElementBase {
  static get is() {
    return 'update-card' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      update: {
        type: Object,
      },

      disabled: {
        type: Boolean,
      },
    };
  }

  update: FirmwareUpdate;
  disabled: boolean;

  protected isCriticalUpdate(): boolean {
    return this.update.priority === UpdatePriority.kCritical;
  }

  protected onUpdateButtonClicked(): void {
    this.dispatchEvent(new CustomEvent<OpenConfirmationDialogEventDetail>(
        'open-confirmation-dialog',
        {bubbles: true, composed: true, detail: {update: this.update}}));
  }

  protected computeVersionText(): string {
    if (!this.update.deviceVersion) {
      return '';
    }

    return this.i18n('versionText', this.update.deviceVersion);
  }

  protected computeDeviceName(): string {
    return mojoString16ToString(this.update.deviceName);
  }

  protected getUpdateButtonA11yLabel(): string {
    return this.i18n('updateButtonA11yLabel', this.computeDeviceName());
  }
}

declare global {
  interface HTMLElementEventMap {
    'open-confirmation-dialog': CustomEvent<OpenConfirmationDialogEventDetail>;
  }

  interface HTMLElementTagNameMap {
    [UpdateCardElement.is]: UpdateCardElement;
  }
}

customElements.define(UpdateCardElement.is, UpdateCardElement);
