// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';

import {LifetimeBrowserProxyImpl} from '/shared/settings/lifetime_browser_proxy.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {assertNotReached} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './relaunch_confirmation_dialog.html.js';
import {RestartType} from './relaunch_mixin.js';

export interface RelaunchConfirmationDialogElement {
  $: {
    cancel: CrButtonElement,
    confirm: CrButtonElement,
    dialog: CrDialogElement,
  };
}

/**
 * The polymer element corresponding to <relaunch-confirmation-dialog>.
 * The dialog is only supported for "non" ChromeOS platforms and is
 * shown to warn users if they have any open Incognito windows before
 * proceeding with the restart/relaunch action.
 *
 * To make use of this dialog, add the below html to the target html and
 * substitute the value of restart-type to either restartTypeEnum.RELAUNCH or
 * restartTypeEnum.RESTART.
 *
 * <template is="dom-if" if="[[shouldShowRelaunchDialog]]" restamp>
 *   <relaunch-confirmation-dialog restart-type="[[restartTypeEnum.RELAUNCH]]"
 * on-close="onRelaunchDialogClose"></relaunch-confirmation-dialog>
 * </template>
 *
 * Then, in the corresponding typescript file, make the target HTMLElement
 * inherit from RelaunchMixin and invoke the member method
 * RelaunchMixin#performRestart where required.
 */
export class RelaunchConfirmationDialogElement extends PolymerElement {
  static get is() {
    return 'relaunch-confirmation-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      relaunchConfirmationDialogDesc: String,

      restartType: Object,

      //  Boolean that defines if the confirmation dialog is opened for browser
      //  version update.
      isVersionUpdate: {
        type: Boolean,
        value: false,
      },
    };
  }

  relaunchConfirmationDialogDesc: string|null;
  restartType: RestartType;
  isVersionUpdate: boolean;

  override async connectedCallback() {
    super.connectedCallback();
    this.relaunchConfirmationDialogDesc =
        await LifetimeBrowserProxyImpl.getInstance()
            .getRelaunchConfirmationDialogDescription(this.isVersionUpdate);
  }

  private onDialogCancel_() {
    this.$.dialog.cancel();
  }

  private onDialogConfirm_() {
    if (RestartType.RELAUNCH === this.restartType) {
      LifetimeBrowserProxyImpl.getInstance().relaunch();
    } else if (RestartType.RESTART === this.restartType) {
      LifetimeBrowserProxyImpl.getInstance().restart();
    } else {
      assertNotReached();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'relaunch-confirmation-dialog': RelaunchConfirmationDialogElement;
  }
}

customElements.define(
    RelaunchConfirmationDialogElement.is, RelaunchConfirmationDialogElement);
