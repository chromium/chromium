// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Dialog that displays APNs for a user to select to be attempted to be used.
 */

import '//resources/ash/common/cr_elements/localized_link/localized_link.js';
import '//resources/ash/common/cr_elements/cr_button/cr_button.js';
import '//resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import '//resources/ash/common/cr_elements/cr_shared_style.css.js';
import '//resources/polymer/v3_0/iron-list/iron-list.js';
import './apn_selection_dialog_list_item.js';

import {I18nMixin} from '//resources/ash/common/cr_elements/i18n_mixin.js';
import {assert} from '//resources/js/assert.js';
import {focusWithoutInk} from '//resources/js/focus_without_ink.js';
import type {ApnProperties, CrosNetworkConfigInterface} from '//resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {afterNextRender, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';

import {getTemplate} from './apn_selection_dialog.html.js';
import {MojoInterfaceProviderImpl} from './mojo_interface_provider.js';

const ApnSelectionDialogElementBase = I18nMixin(PolymerElement);

export class ApnSelectionDialog extends ApnSelectionDialogElementBase {
  static get is() {
    return 'apn-selection-dialog' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      apnList: {
        type: Array,
        value: [],
      },

      /** The GUID of the network to select known APNs for. */
      guid: String,

      shouldOmitLinks: {
        type: Boolean,
        value: false,
      },

      selectedApn_: {
        type: Object,
      },

      /**
       * If |shouldAnnounceA11yActionButtonState_| === true, an a11y
       * announcement will be made. No announcement will be made until the
       * enable state of the action button changes as a result of user changes
       * in the dialog, and subsequent action button state changes (i.e the
       * initial enabled state of the button will not be announced).
       */
      shouldAnnounceA11yActionButtonState_: {
        type: Object,
        value: undefined,
      },

      actionButtonEnabledA11yText_: {
        type: String,
        value: '',
        observer: 'onActionButtonEnabledStateA11yTextChanged_',
        computed:
            'computeActionButtonEnabledStateA11yText_(apnList, selectedApn_)',
      },
    };
  }

  apnList: ApnProperties[];
  guid: string;
  shouldOmitLinks: boolean;
  private selectedApn_: ApnProperties;
  private shouldAnnounceA11yActionButtonState_: boolean|undefined;
  private actionButtonEnabledA11yText_: string;
  private networkConfig_: CrosNetworkConfigInterface;

  constructor() {
    super();

    this.networkConfig_ =
        MojoInterfaceProviderImpl.getInstance().getMojoServiceRemote();
  }

  override connectedCallback() {
    super.connectedCallback();

    // Set the default focus when the dialog opens.
    afterNextRender(this, () => {
      const cancelButton =
          this.shadowRoot!.querySelector<CrButtonElement>('.cancel-button');
      assert(cancelButton);
      focusWithoutInk(cancelButton);

      // Only after dialog is connected and the intended element is focused can
      // action enabled state changes be a11y announced.
      assert(this.shouldAnnounceA11yActionButtonState_ === undefined);
      this.shouldAnnounceA11yActionButtonState_ = false;
    });
  }

  private onCancelClicked_(event: Event): void {
    event.stopPropagation();
    const apnSelectionDialog =
        this.shadowRoot!.querySelector<CrDialogElement>('#apnSelectionDialog');
    assert(apnSelectionDialog);
    if (apnSelectionDialog.open) {
      apnSelectionDialog.close();
    }
  }

  private onActionButtonClicked_(_event: Event): void {
    assert(this.guid);

    if (!this.selectedApn_) {
      return;
    }

    this.networkConfig_.createExclusivelyEnabledCustomApn(
        this.guid, this.selectedApn_);
    const apnSelectionDialog =
        this.shadowRoot!.querySelector<CrDialogElement>('#apnSelectionDialog');
    assert(apnSelectionDialog);
    apnSelectionDialog.close();
  }

  private isApnSelected_(apn: ApnProperties): boolean {
    return apn === this.selectedApn_;
  }

  private computeActionButtonEnabledStateA11yText_(): string {
    return this.selectedApn_ ?
        this.i18n('apnSelectionDialogA11yUseApnEnabled') :
        this.i18n('apnSelectionDialogA11yUseApnDisabled');
  }

  private onActionButtonEnabledStateA11yTextChanged_(
      newVal: string, oldVal: string): void {
    if (this.shouldAnnounceA11yActionButtonState_ === undefined) {
      return;
    }
    if (!newVal || !oldVal) {
      this.shouldAnnounceA11yActionButtonState_ = false;
      return;
    }
    this.shouldAnnounceA11yActionButtonState_ = oldVal !== newVal;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [ApnSelectionDialog.is]: ApnSelectionDialog;
  }
}

customElements.define(ApnSelectionDialog.is, ApnSelectionDialog);
