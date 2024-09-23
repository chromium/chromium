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
import '//resources/ash/common/network/apn_selection_dialog_list_item.js';
import '//resources/polymer/v3_0/iron-list/iron-list.js';

import {assert} from '//resources/ash/common/assert.js';
import {I18nBehavior, I18nBehaviorInterface} from '//resources/ash/common/i18n_behavior.js';
import {focusWithoutInk} from '//resources/js/focus_without_ink.js';
import {ApnProperties, CrosNetworkConfigInterface} from '//resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {afterNextRender, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './apn_selection_dialog.html.js';
import {MojoInterfaceProviderImpl} from './mojo_interface_provider.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const ApnSelectionDialogElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class ApnSelectionDialog extends ApnSelectionDialogElementBase {
  static get is() {
    return 'apn-selection-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** @type {Array<ApnProperties>} */
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

      /** @type {ApnProperties} */
      selectedApn_: {
        type: Object,
      },

      /**
       * If |shouldAnnounceA11yActionButtonState_| === true, an a11y
       * announcement will be made. No announcement will be made until the
       * enable state of the action button changes as a result of user changes
       * in the dialog, and subsequent action button state changes (i.e the
       * initial enabled state of the button will not be announced).
       * @private {boolean|undefined}
       */
      shouldAnnounceA11yActionButtonState_: {
        type: Object,
        value: undefined,
      },

      /** @private */
      actionButtonEnabledA11yText_: {
        type: String,
        value: '',
        observer: 'onActionButtonEnabledStateA11yTextChanged_',
        computed:
            'computeActionButtonEnabledStateA11yText_(apnList, selectedApn_)',
      },
    };
  }

  /** @override */
  constructor() {
    super();

    /** @private {!CrosNetworkConfigInterface} */
    this.networkConfig_ =
        MojoInterfaceProviderImpl.getInstance().getMojoServiceRemote();
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    // Set the default focus when the dialog opens.
    afterNextRender(this, function() {
      focusWithoutInk(this.shadowRoot.querySelector('.cancel-button'));

      // Only after dialog is connected and the intended element is focused can
      // action enabled state changes be a11y announced.
      assert(this.shouldAnnounceA11yActionButtonState_ === undefined);
      this.shouldAnnounceA11yActionButtonState_ = false;
    });
  }

  /**
   * @param {!Event} event
   * @private
   */
  onCancelClicked_(event) {
    event.stopPropagation();
    if (this.$.apnSelectionDialog.open) {
      this.$.apnSelectionDialog.close();
    }
  }

  /**
   * @param {!Event} event
   * @private
   */
  onActionButtonClicked_(event) {
    assert(this.guid);

    if (!this.selectedApn_) {
      return;
    }

    this.networkConfig_.createExclusivelyEnabledCustomApn(
        this.guid, this.selectedApn_);
    this.$.apnSelectionDialog.close();
  }

  /**
   * @param {!ApnProperties} apn
   * @return {boolean}
   * @private
   */
  isApnSelected_(apn) {
    return apn === this.selectedApn_;
  }

  /**
   * @return {string}
   * @private
   */
  computeActionButtonEnabledStateA11yText_() {
    return this.selectedApn_ ?
        this.i18n('apnSelectionDialogA11yUseApnEnabled') :
        this.i18n('apnSelectionDialogA11yUseApnDisabled');
  }

  /**
   * @param {string} newVal
   * @param {string} oldVal
   * @private
   */
  onActionButtonEnabledStateA11yTextChanged_(newVal, oldVal) {
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

customElements.define(ApnSelectionDialog.is, ApnSelectionDialog);
