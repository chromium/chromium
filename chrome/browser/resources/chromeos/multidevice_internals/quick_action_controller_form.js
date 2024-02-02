// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';
import 'chrome://resources/ash/common/cr_elements/md_select.css.js';
import './shared_style.css.js';

import {WebUIListenerBehavior} from 'chrome://resources/ash/common/web_ui_listener_behavior.js';
import {Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MultidevicePhoneHubBrowserProxy} from './multidevice_phonehub_browser_proxy.js';
import {getTemplate} from './quick_action_controller_form.html.js';
import {FindMyDeviceStatus, findMyDeviceStatusToString, TetherStatus, tetherStatusToString} from './types.js';

Polymer({
  is: 'quick-action-controller-form',

  _template: getTemplate(),

  behaviors: [
    WebUIListenerBehavior,
  ],

  properties: {
    /** @private */
    isDndEnabled_: {
      type: Boolean,
      value: false,
      observer: 'enableDnd_',
    },

    /** @private {!TetherStatus} */
    tetherStatus_: {
      type: Number,
      value: TetherStatus.INELIGIBLE_FOR_FEATURE,
    },

    /**
     * Must stay in order with TetherStatus.
     * @private
     */
    tetherStatusList_: {
      type: Array,
      value: () => {
        return [
          TetherStatus.INELIGIBLE_FOR_FEATURE,
          TetherStatus.CONNETION_UNAVAILABLE,
          TetherStatus.CONNECTION_AVAILABLE,
          TetherStatus.CONNECTING,
          TetherStatus.CONNECTED,
          TetherStatus.NO_RECEPTION,
        ];
      },
      readonly: true,
    },

    /** @private {!FindMyDeviceStatus} */
    findMyDeviceStatus_: {
      type: Number,
      value: FindMyDeviceStatus.NOT_AVAILABLE,
    },

    /**
     * Must stay in order with FindMyDeviceStatus.
     * @private
     */
    findMyDeviceStatusList_: {
      type: Array,
      value: () => {
        return [
          FindMyDeviceStatus.NOT_AVAILABLE,
          FindMyDeviceStatus.OFF,
          FindMyDeviceStatus.ON,
        ];
      },
      readonly: true,
    },
  },

  /** @private {?MultidevicePhoneHubBrowserProxy}*/
  browserProxy_: null,

  /** @override */
  created() {
    this.browserProxy_ = MultidevicePhoneHubBrowserProxy.getInstance();
  },

  /** @override */
  attached() {
    this.addWebUIListener(
        'is-dnd-enabled-changed', this.onIsDndEnabledChanged_.bind(this));
    this.addWebUIListener(
        'find-my-device-status-changed',
        this.onFindMyDeviceStatusChanged_.bind(this));
    this.addWebUIListener(
        'tether-status-changed', this.onTetherStatusChanged_.bind(this));
  },

  /** @private */
  enableDnd_() {
    this.browserProxy_.enableDnd(this.isDndEnabled_);
  },

  /** @private */
  setFindMyDeviceStatus_() {
    const select = /** @type {!HTMLSelectElement} */
        (this.$$('#findMyDeviceStatus'));
    this.findMyDeviceStatus_ =
        this.findMyDeviceStatusList_[select.selectedIndex];
    this.browserProxy_.setFindMyDeviceStatus(this.findMyDeviceStatus_);
  },

  /** @private */
  setTetherStatus_() {
    const select = /** @type {!HTMLSelectElement} */
        (this.$$('#tetherStatusList'));
    this.tetherStatus_ = this.tetherStatusList_[select.selectedIndex];
    this.browserProxy_.setTetherStatus(this.tetherStatus_);
  },

  /**
   * @param{boolean} enabled Whether Dnd is enabled.
   * @private
   */
  onIsDndEnabledChanged_(enabled) {
    this.isDndEnabled_ = enabled;
  },

  /**
   * @param{!FindMyDeviceStatus} findMyDeviceStatus The current Find my device
   * status.
   * @private
   */
  onFindMyDeviceStatusChanged_(findMyDeviceStatus) {
    this.findMyDeviceStatus_ = findMyDeviceStatus;
  },

  /**
   * @param{!TetherStatus} tetherStatus The current tether status.
   * @private
   */
  onTetherStatusChanged_(tetherStatus) {
    this.tetherStatus_ = tetherStatus;
  },

  /**
   * @param {!FindMyDeviceStatus} findMyDeviceStatus The ringing status.
   * @private
   */
  getFindMyDeviceStatusName_(findMyDeviceStatus) {
    return findMyDeviceStatusToString.get(findMyDeviceStatus);
  },


  /**
   * @param {!TetherStatus} tetherStatus The status of the feature.
   * @private
   */
  getTetherStatusName_(tetherStatus) {
    return tetherStatusToString.get(tetherStatus);
  },

  /**
   * @param {*} lhs
   * @param {*} rhs
   * @return {boolean}
   * @private
   */
  isEqual_(lhs, rhs) {
    return lhs === rhs;
  },
});
