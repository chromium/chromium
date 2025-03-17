// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * UI element which will show a dialog to create, view or edit APNs.
 */

import '//resources/ash/common/cr_elements/cr_button/cr_button.js';
import '//resources/ash/common/cr_elements/cr_checkbox/cr_checkbox.js';
import '//resources/ash/common/cr_elements/icons.html.js';
import '//resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import '//resources/ash/common/cr_elements/cr_input/cr_input.js';
import '//resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import '//resources/ash/common/cr_elements/cr_expand_button/cr_expand_button.js';
import '//resources/ash/common/cr_elements/cr_shared_style.css.js';
import '//resources/ash/common/cr_elements/md_select.css.js';

import type {CrButtonElement} from '//resources/ash/common/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from '//resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import type {CrInputElement} from '//resources/ash/common/cr_elements/cr_input/cr_input.js';
import {I18nMixin} from '//resources/ash/common/cr_elements/i18n_mixin.js';
import {assert} from '//resources/js/assert.js';
import {focusWithoutInk} from '//resources/js/focus_without_ink.js';
import type {CrosNetworkConfigInterface} from '//resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {ApnAuthenticationType, ApnIpType, ApnState, ApnType} from '//resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {afterNextRender, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './apn_detail_dialog.html.js';
import {ApnDetailDialogMode} from './cellular_utils.js';
import {MojoInterfaceProviderImpl} from './mojo_interface_provider.js';
import type {ApnProperties} from './onc_mojo.js';

const AuthenticationTypes: ApnAuthenticationType[] = [
  ApnAuthenticationType.kAutomatic,
  ApnAuthenticationType.kPap,
  ApnAuthenticationType.kChap,
];

const IpTypes: ApnIpType[] = [
  ApnIpType.kAutomatic,
  ApnIpType.kIpv4,
  ApnIpType.kIpv6,
  ApnIpType.kIpv4Ipv6,
];

enum UiElement {
  INPUT,
  ACTION_BUTTON,
  DONE_BUTTON,
}

/**
 * Regular expression that is used to test for non-ASCII characters.
 */
const APN_NON_ASCII_REGEX = /[^\x00-\x7f]+/;

/**
 * Maximum allowed length of the APN input field.
 */
const MAX_APN_INPUT_LENGTH = 63;

const ApnDetailDialogBase = I18nMixin(PolymerElement);

export class ApnDetailDialog extends ApnDetailDialogBase {
  static get is() {
    return 'apn-detail-dialog' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      apnProperties: {
        type: Object,
        observer: 'onApnPropertiesUpdated_',
      },

      mode: {
        type: Object,
        value: ApnDetailDialogMode.CREATE,
      },

      guid: {type: String},

      apnList: {
        type: Array,
        value: [],
      },

      advancedSettingsExpanded_: {
        type: Boolean,
        value: false,
      },

      AuthenticationTypes: {
        type: Array,
        value: AuthenticationTypes,
        readOnly: true,
      },

      IpTypes: {
        type: Array,
        value: IpTypes,
        readOnly: true,
      },

      /**
       * Enum used as an ID for specific UI elements.
       */
      UiElement: {
        type: Object,
        value: UiElement,
      },

      selectedAuthType_: {
        type: String,
        value: AuthenticationTypes[0].toString(),
      },

      selectedIpType_: {
        type: String,
        value: IpTypes[0].toString(),
      },

      apn_: {
        type: String,
        value: '',
        observer: 'onApnValueChanged_',
      },

      username_: {
        type: String,
        value: '',
      },

      password_: {
        type: String,
        value: '',
      },

      isDefaultApnType_: {
        type: Boolean,
        value: true,
      },

      isAttachApnType_: {
        type: Boolean,
        value: false,
      },

      isApnInputInvalid_: {
        type: Boolean,
        value: false,
        computed:
            'computeIsApnInputInvalid_(apn_, isMaxApnInputLengthReached_)',
      },

      isMaxApnInputLengthReached_: {
        type: Boolean,
        value: false,
      },

      shouldShowApnTypeErrorMessage_: {
        type: Boolean,
        value: false,
        computed: 'computeShouldShowApnTypeErrorMessage_(apnList, ' +
            'isDefaultApnType_, isAttachApnType_)',
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
        computed: 'computeActionButtonEnabledStateA11yText_(apn_, ' +
            'isMaxApnInputLengthReached_, shouldShowApnTypeErrorMessage_,' +
            'isDefaultApnType_, isAttachApnType_)',
      },
    };
  }

  apnProperties: ApnProperties|undefined;
  mode: ApnDetailDialogMode;
  guid: string;
  apnList: ApnProperties[];
  private advancedSettingsExpanded_: boolean;
  private AuthenticationTypes: ApnAuthenticationType[];
  private IpTypes: ApnIpType[];
  private UiElement: UiElement;
  private selectedAuthType_: string;
  private selectedIpType_: string;
  private apn_: string;
  private username_: string;
  private password_: string;
  private isDefaultApnType_: boolean;
  private isAttachApnType_: boolean;
  private isApnInputInvalid_: boolean;
  private isMaxApnInputLengthReached_: boolean;
  private shouldShowApnTypeErrorMessage_: boolean;
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
      switch (this.mode) {
        case ApnDetailDialogMode.CREATE:
        case ApnDetailDialogMode.EDIT:
          const input =
              this.shadowRoot!.querySelector<CrInputElement>('cr-input');
          if (input) {
            focusWithoutInk(input);
          }
          break;
        case ApnDetailDialogMode.VIEW:
          const doneBtn =
              this.shadowRoot!.querySelector<CrButtonElement>('#apnDoneBtn');
          if (doneBtn) {
            focusWithoutInk(doneBtn);
          }
          break;
      }

      // Only after dialog is connected and the intended element is focused can
      // action enabled state changes be a11y announced.
      assert(this.shouldAnnounceA11yActionButtonState_ === undefined);
      this.shouldAnnounceA11yActionButtonState_ = false;
    });
  }

  /**
   * Observer method used to fill the apn detail dialog, with the provided apn.
   */
  private onApnPropertiesUpdated_(): void {
    this.apn_ = this.apnProperties.accessPointName;
    this.username_ = this.apnProperties.username;
    this.password_ = this.apnProperties.password;
    this.selectedIpType_ = this.apnProperties.ipType.toString();
    this.selectedAuthType_ = this.apnProperties.authentication.toString();
    this.isDefaultApnType_ = false;
    this.isAttachApnType_ = false;

    for (const apnType of this.apnProperties.apnTypes) {
      if (apnType === ApnType.kDefault) {
        this.isDefaultApnType_ = true;
      } else if (apnType === ApnType.kAttach) {
        this.isAttachApnType_ = true;
      }
    }
  }

  /**
   * Observer for apn_ that is used for detecting whether the max apn length
   * was reached or not and truncating it to MAX_APN_INPUT_LENGTH if so.
   */
  private onApnValueChanged_(_: string, oldValue: string): void {
    if (oldValue) {
      // If oldValue.length > MAX_APN_INPUT_LENGTH, the user attempted to
      // enter more than the max limit, this method was called and it was
      // truncated, and then this method was called one more time.
      this.isMaxApnInputLengthReached_ = oldValue.length > MAX_APN_INPUT_LENGTH;
    } else {
      this.isMaxApnInputLengthReached_ = false;
    }

    // Truncate the name to MAX_INPUT_LENGTH.
    this.apn_ = this.apn_.substring(0, MAX_APN_INPUT_LENGTH);
  }

  /**
   * Computes whether the APN type error message should be shown or not. It
   * should be shown when the user tries to get into a state where no enabled
   * default APNs but still one or more enabled attach APNs.
   */
  private computeShouldShowApnTypeErrorMessage_(): boolean {
    // APN type is always valid if the default APN type is checked.
    if (this.isDefaultApnType_) {
      return false;
    }
    const enabledDefaultApns = this.apnList.filter(
        properties => properties.state === ApnState.kEnabled &&
            properties.apnTypes.includes(ApnType.kDefault));
    const enabledAttachApns = this.apnList.filter(
        properties => properties.state === ApnState.kEnabled &&
            properties.apnTypes.includes(ApnType.kAttach));
    switch (this.mode) {
      case ApnDetailDialogMode.CREATE:
        // If there are no default enabled APNs and the user checks the
        // attach APN checkbox then the APN type error message should be shown.
        return enabledDefaultApns.length === 0 && this.isAttachApnType_;
      case ApnDetailDialogMode.EDIT:
        // If there is an enabled default APN other than the current one being
        // edited, then the APN type error message should not be shown.
        if (enabledDefaultApns.some(apn => apn.id !== this.apnProperties.id)) {
          return false;
        }
        // The APN being edited is the only enabled default APN and the user
        // unchecks the default checkbox and checks the attach checkbox then
        // the APN type error message should be shown.
        if (this.isAttachApnType_) {
          return true;
        }
        // The APN being edited is the only enabled default APN but there are
        // other enabled attach APNs and the user unchecks the default
        // checkbox.
        if (enabledAttachApns.some(apn => apn.id !== this.apnProperties.id)) {
          return true;
        }
    }
    return false;
  }

  private computeIsApnInputInvalid_(): boolean {
    return this.isMaxApnInputLengthReached_ ||
        APN_NON_ASCII_REGEX.test(this.apn_);
  }

  private getApnErrorMessage_(): string {
    if (!this.isApnInputInvalid_) {
      return '';
    }
    if (this.isMaxApnInputLengthReached_) {
      return this.i18n('apnDetailApnErrorMaxChars', MAX_APN_INPUT_LENGTH);
    }
    return this.i18n('apnDetailApnErrorInvalidChar');
  }

  private onCancelClicked_(event: Event): void {
    event.stopPropagation();
    const dialog =
        this.shadowRoot!.querySelector<CrDialogElement>('#apnDetailDialog')!;
    if (dialog.open) {
      dialog.close();
    }
  }

  private onActionButtonClicked_(): void {
    assert(this.guid);
    assert(this.mode !== ApnDetailDialogMode.VIEW);
    if (this.mode === ApnDetailDialogMode.CREATE) {
      // Note: apnProperties is undefined when we are in the create mode.
      assert(!this.apnProperties);
      this.networkConfig_.createCustomApn(this.guid, this.getApnProperties_());
    } else if (this.mode === ApnDetailDialogMode.EDIT) {
      assert(!!this.apnProperties.id);
      this.networkConfig_.modifyCustomApn(
          this.guid, this.getApnProperties_(this.apnProperties));
    }
    this.shadowRoot!.querySelector<CrDialogElement>(
                        '#apnDetailDialog')!.close();
  }

  private getApnProperties_(apnProperties: ApnProperties = {}): ApnProperties {
    apnProperties.accessPointName = this.apn_;
    apnProperties.username = this.username_;
    apnProperties.password = this.password_;
    apnProperties.authentication = Number(this.selectedAuthType_);
    apnProperties.ipType = Number(this.selectedIpType_);
    // TODO(b/162365553): Check that ApnTypes is non-empty
    apnProperties.apnTypes = this.getSelectedApnTypes_();
    return apnProperties;
  }

  private getActionButtonTitle_(): string {
    if (this.mode === ApnDetailDialogMode.EDIT) {
      return this.i18n('apnDetailDialogSave');
    }
    return this.i18n('apnDetailDialogAdd');
  }

  private computeActionButtonEnabledStateA11yText_(): string {
    const isDisabled = this.isUiElementDisabled_(UiElement.ACTION_BUTTON);
    if (this.mode === ApnDetailDialogMode.EDIT) {
      return isDisabled ? this.i18n('apnDetailDialogA11ySaveDisabled') :
                          this.i18n('apnDetailDialogA11ySaveEnabled');
    } else if (this.mode === ApnDetailDialogMode.CREATE) {
      return isDisabled ? this.i18n('apnDetailDialogA11yAddDisabled') :
                          this.i18n('apnDetailDialogA11yAddEnabled');
    }
    return '';
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

  private getDialogTitle_(): string {
    switch (this.mode) {
      case ApnDetailDialogMode.CREATE:
        return this.i18n('apnDetailAddApnDialogTitle');
      case ApnDetailDialogMode.VIEW:
        return this.i18n('apnDetailViewApnDialogTitle');
      case ApnDetailDialogMode.EDIT:
        return this.i18n('apnDetailEditApnDialogTitle');
    }
    return '';
  }
  /**
   * Maps the checkboxes to an array of {@link ApnType}.
   */
  private getSelectedApnTypes_(): ApnType[] {
    const apnTypes = [];
    if (this.isDefaultApnType_) {
      apnTypes.push(ApnType.kDefault);
    }

    if (this.isAttachApnType_) {
      apnTypes.push(ApnType.kAttach);
    }
    return apnTypes;
  }

  /**
   * Returns the localized label for the auth type.
   */
  private getAuthTypeLocalizedLabel_(auth_type: ApnAuthenticationType): string {
    switch (auth_type) {
      case ApnAuthenticationType.kAutomatic:
        return this.i18n('apnDetailTypeAuto');
      case ApnAuthenticationType.kChap:
        return this.i18n('apnDetailAuthTypeCHAP');
      case ApnAuthenticationType.kPap:
        return this.i18n('apnDetailAuthTypePAP');
    }
  }

  /**
   * Returns the localized label for the ip type.
   */
  private getIpTypeLocalizedLabel_(ip_type: ApnIpType): string {
    switch (ip_type) {
      case ApnIpType.kAutomatic:
        return this.i18n('apnDetailTypeAuto');
      case ApnIpType.kIpv4:
        return this.i18n('apnDetailIpTypeIpv4');
      case ApnIpType.kIpv6:
        return this.i18n('apnDetailIpTypeIpv6');
      case ApnIpType.kIpv4Ipv6:
        return this.i18n('apnDetailIpTypeIpv4_Ipv6');
    }
  }

  private isSelectedIpType_(item: number): boolean {
    return Number(this.selectedIpType_) === item;
  }

  private isSelectedAuthType_(item: number): boolean {
    return Number(this.selectedAuthType_) === item;
  }

  private isUiElementDisabled_(uiElement: UiElement): boolean {
    switch (uiElement) {
      case UiElement.INPUT:
        return this.mode === ApnDetailDialogMode.VIEW;
      case UiElement.ACTION_BUTTON:
        return this.apn_.length === 0 || this.isApnInputInvalid_ ||
            this.shouldShowApnTypeErrorMessage_ ||
            (!this.isDefaultApnType_ && !this.isAttachApnType_);
    }
    return false;
  }

  private isUiElementVisible_(uiElement: UiElement): boolean {
    switch (uiElement) {
      case UiElement.DONE_BUTTON:
        return this.mode === ApnDetailDialogMode.VIEW;
      case UiElement.ACTION_BUTTON:
        return this.mode === ApnDetailDialogMode.CREATE ||
            this.mode === ApnDetailDialogMode.EDIT;
    }
    return true;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [ApnDetailDialog.is]: ApnDetailDialog;
  }
}

customElements.define(ApnDetailDialog.is, ApnDetailDialog);
