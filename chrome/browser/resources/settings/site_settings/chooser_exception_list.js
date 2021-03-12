// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'chooser-exception-list' shows a list of chooser exceptions for a given
 * chooser type.
 */
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';
import '../settings_shared_css.js';
import './chooser_exception_list_entry.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {ListPropertyUpdateBehavior} from 'chrome://resources/js/list_property_update_behavior.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';

import {ChooserType, ContentSettingsTypes} from './constants.js';
import {SiteSettingsBehavior} from './site_settings_behavior.js';
import {ChooserException, RawChooserException} from './site_settings_prefs_browser_proxy.js';

Polymer({
  is: 'chooser-exception-list',

  _template: html`{__html_template__}`,

  behaviors: [
    I18nBehavior,
    ListPropertyUpdateBehavior,
    SiteSettingsBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /**
     * Array of chooser exceptions to display in the widget.
     * @type {!Array<ChooserException>}
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
     * @type {!ChooserType}
     */
    chooserType: {
      observer: 'chooserTypeChanged_',
      type: String,
      value: ChooserType.NONE,
    },

    /** @private */
    emptyListMessage_: {
      type: String,
      value: '',
    },

    /** @private */
    hasIncognito_: Boolean,

    /** @private */
    tooltipText_: String,
  },

  /** @override */
  attached() {
    this.addWebUIListener(
        'contentSettingChooserPermissionChanged',
        this.objectWithinChooserTypeChanged_.bind(this));
    this.addWebUIListener(
        'onIncognitoStatusChanged', this.onIncognitoStatusChanged_.bind(this));
    this.browserProxy.updateIncognitoStatus();
  },

  /**
   * Called when a chooser exception changes permission and updates the element
   * if |category| is equal to the settings category of this element.
   * @param {ContentSettingsTypes} category The content settings type
   *     that represents this permission category.
   * @param {ChooserType} chooserType The content settings type that
   *     represents the chooser data for this permission.
   * @private
   */
  objectWithinChooserTypeChanged_(category, chooserType) {
    if (category === this.category && chooserType === this.chooserType) {
      this.chooserTypeChanged_();
    }
  },

  /**
   * Called for each chooser-exception-list when incognito is enabled or
   * disabled. Only called on change (opening N incognito windows only fires one
   * message). Another message is sent when the *last* incognito window closes.
   * @private
   */
  onIncognitoStatusChanged_(hasIncognito) {
    this.hasIncognito_ = hasIncognito;
    this.populateList_();
  },

  /**
   * Configures the visibility of the widget and shows the list.
   * @private
   */
  chooserTypeChanged_() {
    if (this.chooserType === ChooserType.NONE) {
      return;
    }

    // Set the message to display when the exception list is empty.
    switch (this.chooserType) {
      case ChooserType.USB_DEVICES:
        this.emptyListMessage_ = this.i18n('noUsbDevicesFound');
        break;
      case ChooserType.SERIAL_PORTS:
        this.emptyListMessage_ = this.i18n('noSerialPortsFound');
        break;
      case ChooserType.HID_DEVICES:
        this.emptyListMessage_ = this.i18n('noHidDevicesFound');
        break;
      case ChooserType.BLUETOOTH_DEVICES:
        this.emptyListMessage_ = this.i18n('noBluetoothDevicesFound');
        break;
      default:
        this.emptyListMessage_ = '';
    }

    this.populateList_();
  },

  /**
   * Returns true if there are any chooser exceptions for this chooser type.
   * @return {boolean}
   * @private
   */
  hasExceptions_() {
    return this.chooserExceptions.length > 0;
  },

  /**
   * Need to use a common tooltip since the tooltip in the entry is cut off from
   * the iron-list.
   * @param{!CustomEvent<!{target: HTMLElement, text: string}>} e
   * @private
   */
  onShowTooltip_(e) {
    this.tooltipText_ = e.detail.text;
    const target = e.detail.target;
    // paper-tooltip normally determines the target from the |for| property,
    // which is a selector. Here paper-tooltip is being reused by multiple
    // potential targets.
    this.$.tooltip.target = target;
    const hide = () => {
      this.$.tooltip.hide();
      target.removeEventListener('mouseleave', hide);
      target.removeEventListener('blur', hide);
      target.removeEventListener('click', hide);
      this.$.tooltip.removeEventListener('mouseenter', hide);
    };
    target.addEventListener('mouseleave', hide);
    target.addEventListener('blur', hide);
    target.addEventListener('click', hide);
    this.$.tooltip.addEventListener('mouseenter', hide);
    this.$.tooltip.show();
  },

  /**
   * Populate the chooser exception list for display.
   * @private
   */
  populateList_() {
    this.browserProxy.getChooserExceptionList(this.chooserType)
        .then(exceptionList => this.processExceptions_(exceptionList));
  },

  /**
   * Process the chooser exception list returned from the native layer.
   * @param {!Array<RawChooserException>} exceptionList
   * @private
   */
  processExceptions_(exceptionList) {
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
      const siteUidGetter = x => x.origin + x.embeddingOrigin + x.incognito;
      exceptions.forEach((exception, index) => {
        const propertyPath = 'chooserExceptions.' + index + '.sites';
        this.updateList(propertyPath, siteUidGetter, exception.sites);
      }, this);
    }
  },
});
