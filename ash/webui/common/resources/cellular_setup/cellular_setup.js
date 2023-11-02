// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Root element for the cellular setup flow. This element wraps
 * the psim setup flow, esim setup flow, and setup flow selection page.
 */
import './button_bar.js';
import './psim_flow_ui.js';
import './esim_flow_ui.js';

import {I18nBehavior} from '//resources/ash/common/i18n_behavior.js';
import {Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './cellular_setup.html.js';
import {CellularSetupDelegate} from './cellular_setup_delegate.js';
import {ButtonBarState, CellularSetupPageName} from './cellular_types.js';

Polymer({
  _template: getTemplate(),
  is: 'cellular-setup',

  behaviors: [I18nBehavior],

  properties: {
    /** @type {!CellularSetupDelegate} */
    delegate: Object,

    /**
     * Title of the flow, shown at the top of the dialog. No title shown if the
     * string is empty.
     */
    flowTitle: {
      type: String,
      notify: true,
      value: '',
    },

    /**
     * Header for the flow, shown below the title. No header shown if the string
     * is empty.
     */
    flowHeader: {
      type: String,
      notify: true,
      value: '',
    },

    /**
     * Name of the currently displayed sub-page.
     * @private {!CellularSetupPageName|null}
     */
    currentPageName: String,

    /**
     * Current user selected setup flow page name.
     * @private {!CellularSetupPageName|null}
     */
    selectedFlow_: {
      type: String,
      value: null,
    },

    /**
     * Button bar button state.
     * @private {!ButtonBarState}
     */
    buttonState_: {
      type: Object,
      notify: true,
    },

    /**
     * DOM Element corresponding to the visible page.
     *
     * @private {!PsimFlowUiElement|!EsimFlowUiElement}
     */
    currentPage_: {
      type: Object,
      observer: 'onPageChange_',
    },

    /**
     * Text for the button_bar's 'Forward' button.
     * @private {string}
     */
    forwardButtonLabel_: {
      type: String,
    },
  },

  listeners: {
    'backward-nav-requested': 'onBackwardNavRequested_',
    'retry-requested': 'onRetryRequested_',
    'forward-nav-requested': 'onForwardNavRequested_',
    'cancel-requested': 'onCancelRequested_',
    'focus-default-button': 'onFocusDefaultButton_',
  },


  /** @override */
  attached() {
    // By default eSIM flow is selected.
    if (!this.currentPageName) {
      this.currentPageName = CellularSetupPageName.ESIM_FLOW_UI;
    }
  },

  /** @private */
  onPageChange_() {
    if (this.currentPage_) {
      this.flowTitle = '';
      this.currentPage_.initSubflow();
    }
  },

  /** @private */
  onBackwardNavRequested_() {
    this.currentPage_.navigateBackward();
  },

  onCancelRequested_() {
    this.fire('exit-cellular-setup');
  },

  /** @private */
  onRetryRequested_() {
    // TODO(crbug.com/1093185): Add try again logic.
  },

  /** @private */
  onForwardNavRequested_() {
    this.currentPage_.navigateForward();
  },

  /** @private */
  onFocusDefaultButton_() {
    this.$.buttonBar.focusDefaultButton();
  },

  /**
   * @param {string} currentPage
   * @private
   */
  shouldShowPsimFlow_(currentPage) {
    return currentPage === CellularSetupPageName.PSIM_FLOW_UI;
  },

  /**
   * @param {string} currentPage
   * @private
   */
  shouldShowEsimFlow_(currentPage) {
    return currentPage === CellularSetupPageName.ESIM_FLOW_UI;
  },
});
