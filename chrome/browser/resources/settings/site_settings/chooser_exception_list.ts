// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'chooser-exception-list' shows a list of chooser exceptions for a given
 * chooser type.
 */
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/cr_tooltip/cr_tooltip.js';
import '../settings_shared.css.js';
import '../i18n_setup.js';
import './chooser_exception_list_entry.js';

import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {ListPropertyUpdateMixin} from 'chrome://resources/cr_elements/list_property_update_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import type {CrTooltipElement} from 'chrome://resources/cr_elements/cr_tooltip/cr_tooltip.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {TooltipMixin} from '../tooltip_mixin.js';

import {getTemplate} from './chooser_exception_list.html.js';
import type {ContentSettingsTypes} from './constants.js';
import {ChooserType} from './constants.js';
import {SiteSettingsMixin} from './site_settings_mixin.js';
import type {ChooserException, RawChooserException, SiteException} from './site_settings_prefs_browser_proxy.js';

export interface ChooserExceptionListElement {
  $: {
    confirmResetSettings: CrDialogElement,
    tooltip: CrTooltipElement,
  };
}

const ChooserExceptionListElementBase = TooltipMixin(ListPropertyUpdateMixin(
    SiteSettingsMixin(WebUiListenerMixin(I18nMixin(PolymerElement)))));

export class ChooserExceptionListElement extends
    ChooserExceptionListElementBase {
  static get is() {
    return 'chooser-exception-list';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Array of chooser exceptions to display in the widget.
       */
      chooserExceptions: {
        type: Array,
        value() {
          return [];
        },
      },

      /**
       * The string ID of the chooser type that this element is displaying data
       * for.
       * See site_settings/constants.js for possible values.
       */
      chooserType: {
        observer: 'chooserTypeChanged_',
        type: String,
        value: ChooserType.NONE,
      },

      emptyListMessage_: {
        type: String,
        value: '',
      },

      hasIncognito_: Boolean,

      resetPermissionsMessage_: {
        type: String,
        value: '',
      },

      tooltipText_: String,
    };
  }

  chooserExceptions: ChooserException[];
  chooserType: ChooserType;
  private emptyListMessage_: string;
  private hasIncognito_: boolean;
  private resetPermissionsMessage_: string;
  private tooltipText_: string;

  override connectedCallback() {
    super.connectedCallback();

    this.addWebUiListener(
        'contentSettingChooserPermissionChanged',
        (category: ContentSettingsTypes, chooserType: ChooserType) => {
          this.objectWithinChooserTypeChanged_(category, chooserType);
        });
    this.addWebUiListener(
        'onIncognitoStatusChanged',
        (hasIncognito: boolean) =>
            this.onIncognitoStatusChanged_(hasIncognito));
    this.browserProxy.updateIncognitoStatus();
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
  private onIncognitoStatusChanged_(hasIncognito: boolean) {
    this.hasIncognito_ = hasIncognito;
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
      default:
        this.emptyListMessage_ = '';
        this.resetPermissionsMessage_ = '';
    }

    this.populateList_();
  }

  /**
   * @return true if there are any chooser exceptions for this chooser type.
   */
  private hasExceptions_(): boolean {
    return this.chooserExceptions.length > 0;
  }

  /**
   * Need to use a common tooltip since the tooltip in the entry is cut off from
   * the iron-list.
   */
  private onShowTooltip_(e: CustomEvent<{target: HTMLElement, text: string}>) {
    this.tooltipText_ = e.detail.text;
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
    const exceptions = exceptionList.map(exception => {
      const sites = exception.sites.map(site => this.expandSiteException(site));
      return Object.assign(exception, {sites});
    });

    if (!this.updateList(
            'chooserExceptions', x => x.displayName, exceptions,
            true /* identityBasedUpdate= */)) {
      // The chooser objects have not been changed, so check if their site
      // permissions have changed. The |exceptions| and |this.chooserExceptions|
      // arrays should be the same length.
      const siteUidGetter = (x: SiteException) =>
          x.origin + x.embeddingOrigin + x.incognito;
      exceptions.forEach((exception, index) => {
        const propertyPath = 'chooserExceptions.' + index + '.sites';
        this.updateList(propertyPath, siteUidGetter, exception.sites);
      }, this);
    }
  }

  /**
   * Confirms the resetting of all content settings for an origin.
   */
  private onConfirmClearSettings_(e: Event) {
    e.preventDefault();
    this.$.confirmResetSettings.showModal();
  }

  private onCloseDialog_(e: Event) {
    (e.target as HTMLElement).closest('cr-dialog')!.close();
  }

  /**
   * Resets all permissions for the current origin.
   */
  private onResetSettings_(e: Event) {
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
