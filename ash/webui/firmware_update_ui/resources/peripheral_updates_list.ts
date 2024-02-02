// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-webui.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
import './firmware_shared.css.js';
import './firmware_shared_fonts.css.js';
import './firmware_update.mojom-webui.js';
import './update_card.js';
import './strings.m.js';

import {I18nMixin, I18nMixinInterface} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {IronA11yAnnouncer} from 'chrome://resources/polymer/v3_0/iron-a11y-announcer/iron-a11y-announcer.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {FirmwareUpdate, UpdateObserverInterface, UpdateObserverReceiver} from './firmware_update.mojom-webui.js';
import {IronAnnounceEventDetail, OpenUpdateDialogEventDetail} from './firmware_update_types.js';
import {getUpdateProvider} from './mojo_interface_provider.js';
import {getTemplate} from './peripheral_updates_list.html.js';

/**
 * @fileoverview
 * 'peripheral-updates-list' displays a list of available peripheral updates.
 */

const PeripheralUpdateListElementBase =
    I18nMixin(PolymerElement) as {new (): I18nMixinInterface & PolymerElement};

export class PeripheralUpdateListElement extends PeripheralUpdateListElementBase
    implements UpdateObserverInterface {
  static get is() {
    return 'peripheral-updates-list' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      firmwareUpdates: {
        type: Array,
        value: () => [],
      },

      hasCheckedInitialInflightProgress: {
        type: Boolean,
        value: false,
      },
    };
  }

  protected firmwareUpdates: FirmwareUpdate[];
  protected hasCheckedInitialInflightProgress: boolean;
  private updateProvider = getUpdateProvider();
  protected updateListObserverReceiver: UpdateObserverReceiver;

  constructor() {
    super();

    this.observePeripheralUpdates();
  }

  private observePeripheralUpdates(): void {
    // Calling observePeripheralUpdates will trigger onUpdateListChanged.
    this.updateListObserverReceiver = new UpdateObserverReceiver(this);

    this.updateProvider.observePeripheralUpdates(
        this.updateListObserverReceiver.$.bindNewPipeAndPassRemote());
  }

  /** Implements UpdateObserver.onUpdateListChanged */
  onUpdateListChanged(firmwareUpdates: FirmwareUpdate[]): void {
    this.firmwareUpdates = firmwareUpdates;
    this.announceNumUpdates();
    if (!this.hasCheckedInitialInflightProgress) {
      this.updateProvider.fetchInProgressUpdate().then(result => {
        if (result.update) {
          this.dispatchEvent(new CustomEvent<OpenUpdateDialogEventDetail>(
              'open-update-dialog', {
                bubbles: true,
                composed: true,
                detail: {update: result.update, inflight: true},
              }));
        }
        this.hasCheckedInitialInflightProgress = true;
      });
    }
  }

  protected hasFirmwareUpdates(): boolean {
    return this.firmwareUpdates.length > 0;
  }

  protected announceNumUpdates(): void {
    IronA11yAnnouncer.requestAvailability();
    this.dispatchEvent(
        new CustomEvent<IronAnnounceEventDetail>('iron-announce', {
          bubbles: true,
          composed: true,
          detail:
              {text: this.i18n('numUpdatesText', this.firmwareUpdates.length)},
        }));
  }

  getFirmwareUpdatesForTesting(): FirmwareUpdate[] {
    return this.firmwareUpdates;
  }

  setFirmwareUpdatesForTesting(updates: FirmwareUpdate[]): void {
    this.firmwareUpdates = updates;
  }
}

declare global {
  interface HTMLElementEventMap {
    'iron-announce': CustomEvent<IronAnnounceEventDetail>;
    'open-update-dialog': CustomEvent<OpenUpdateDialogEventDetail>;
  }

  interface HTMLElementTagNameMap {
    [PeripheralUpdateListElement.is]: PeripheralUpdateListElement;
  }
}

customElements.define(
    PeripheralUpdateListElement.is, PeripheralUpdateListElement);
