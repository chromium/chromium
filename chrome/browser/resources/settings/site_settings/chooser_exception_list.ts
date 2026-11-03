// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'chooser-exception-list' shows a list of chooser exceptions for a given
 * chooser type.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_tooltip/cr_tooltip.js';
import './chooser_exception_list_entry.js';

import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrTooltipElement} from 'chrome://resources/cr_elements/cr_tooltip/cr_tooltip.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {TooltipMixinLit} from '../tooltip_mixin_lit.js';

import {getCss} from './chooser_exception_list.css.js';
import {getHtml} from './chooser_exception_list.html.js';
import type {ContentSettingsTypes} from './constants.js';
import {ChooserType} from './constants.js';
import type {ChooserException, RawChooserException} from './site_settings_browser_proxy.js';
import {SiteSettingsMixinLit} from './site_settings_mixin_lit.js';

export interface ChooserExceptionListElement {
  $: {
    confirmResetSettings: CrDialogElement,
    tooltip: CrTooltipElement,
  };
}

const ChooserExceptionListElementBase = TooltipMixinLit(
    SiteSettingsMixinLit(WebUiListenerMixinLit(I18nMixinLit(CrLitElement))));

export class ChooserExceptionListElement extends
    ChooserExceptionListElementBase {
  static get is() {
    return 'chooser-exception-list';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /**
       * Array of chooser exceptions to display in the widget.
       */
      chooserExceptions: {type: Array},

      /**
       * The string ID of the chooser type that this element is displaying data
       * for.
       * See site_settings/constants.js for possible values.
       */
      chooserType: {type: String},

      emptyListMessage_: {type: String},
      resetPermissionsMessage_: {type: String},
      tooltipText_: {type: String},
    };
  }

  accessor chooserExceptions: ChooserException[] = [];
  accessor chooserType: ChooserType = ChooserType.NONE;
  protected accessor emptyListMessage_: string = '';
  protected accessor resetPermissionsMessage_: string = '';
  protected accessor tooltipText_: string = '';

  override connectedCallback() {
    super.connectedCallback();

    this.addWebUiListener(
        'contentSettingChooserPermissionChanged',
        (category: ContentSettingsTypes, chooserType: ChooserType) => {
          this.objectWithinChooserTypeChanged_(category, chooserType);
        });
    this.addWebUiListener(
        'onIncognitoStatusChanged', () => this.onIncognitoStatusChanged_());
    this.browserProxy.updateIncognitoStatus();
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('chooserType')) {
      this.chooserTypeChanged_();
    }
  }

  /**
   * Called when a chooser exception changes permission and updates the element
   * if |category| is equal to the settings category of this element.
   * @param category The content settings type that represents this permission
   *     category.
   * @param chooserType The content settings type that represents the chooser
   *     data for this permission.
   */
  private objectWithinChooserTypeChanged_(
      category: ContentSettingsTypes, chooserType: ChooserType) {
    if (category === this.category && chooserType === this.chooserType) {
      this.chooserTypeChanged_();
    }
  }

  /**
   * Called for each chooser-exception-list when incognito is enabled or
   * disabled. Only called on change (opening N incognito windows only fires one
   * message). Another message is sent when the *last* incognito window closes.
   */
  private onIncognitoStatusChanged_() {
    this.populateList_();
  }

  /**
   * Configures the visibility of the widget and shows the list.
   */
  private chooserTypeChanged_() {
    if (this.chooserType === ChooserType.NONE) {
      return;
    }

    // Set the message to display when the exception list is empty.
    switch (this.chooserType) {
      case ChooserType.USB_DEVICES:
        this.emptyListMessage_ = this.i18n('noUsbDevicesFound');
        this.resetPermissionsMessage_ = this.i18n('resetUsbConfirmation');
        break;
      case ChooserType.SERIAL_PORTS:
        this.emptyListMessage_ = this.i18n('noSerialPortsFound');
        this.resetPermissionsMessage_ =
            this.i18n('resetSerialPortsConfirmation');
        break;
      case ChooserType.HID_DEVICES:
        this.emptyListMessage_ = this.i18n('noHidDevicesFound');
        this.resetPermissionsMessage_ = this.i18n('resetHidConfirmation');
        break;
      case ChooserType.BLUETOOTH_DEVICES:
        this.emptyListMessage_ = this.i18n('noBluetoothDevicesFound');
        this.resetPermissionsMessage_ = this.i18n('resetBluetoothConfirmation');
        break;
      // <if expr="enable_smart_card">
      case ChooserType.SMART_CARD_READERS_DEVICES:
        this.emptyListMessage_ =
            this.i18n('siteSettingsNoSmartCardReadersFound');
        this.resetPermissionsMessage_ =
            this.i18n('siteSettingsResetSmartCardConfirmation');
        break;
      // </if>
      default:
        this.emptyListMessage_ = '';
        this.resetPermissionsMessage_ = '';
    }

    this.populateList_();
  }

  /**
   * @return true if there are any chooser exceptions for this chooser type.
   */
  protected hasExceptions_(): boolean {
    return this.chooserExceptions.length > 0;
  }

  protected async onShowTooltip_(
      e: CustomEvent<{target: HTMLElement, text: string}>) {
    this.tooltipText_ = e.detail.text;
    await this.updateComplete;
    // cr-tooltip normally determines the target from the |for| property,
    // which is a selector. Here cr-tooltip is being reused by multiple
    // potential targets.
    this.showTooltipAtTarget(this.$.tooltip, e.detail.target);
  }

  /**
   * Populate the chooser exception list for display.
   */
  private populateList_() {
    this.browserProxy.getChooserExceptionList(this.chooserType)
        .then(exceptionList => this.processExceptions_(exceptionList));
  }

  /**
   * Process the chooser exception list returned from the native layer.
   */
  private processExceptions_(exceptionList: RawChooserException[]) {
    this.chooserExceptions = exceptionList.map(exception => {
      const sites = exception.sites.map(site => this.expandSiteException(site));
      return {...exception, sites};
    });
  }

  /**
   * Opens the dialog that confirms resetting all chooser exceptions for this
   * chooser type.
   */
  protected onConfirmClearSettingsClick_(e: Event) {
    e.preventDefault();
    this.$.confirmResetSettings.showModal();
  }

  private onCloseDialog_(e: Event) {
    (e.target as HTMLElement).closest('cr-dialog')!.close();
  }

  protected onCloseDialogClick_(e: Event) {
    this.onCloseDialog_(e);
  }

  /**
   * Resets all chooser exceptions for this chooser type.
   */
  protected onResetSettingsClick_(e: Event) {
    this.chooserExceptions.forEach(exception => {
      exception.sites.forEach(site => {
        this.browserProxy.resetChooserExceptionForSite(
            exception.chooserType, site.origin, exception.object);
      });
    });

    this.onCloseDialog_(e);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'chooser-exception-list': ChooserExceptionListElement;
  }
}

customElements.define(
    ChooserExceptionListElement.is, ChooserExceptionListElement);
