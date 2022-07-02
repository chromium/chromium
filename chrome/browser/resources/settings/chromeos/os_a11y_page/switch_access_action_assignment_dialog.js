// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This dialog holds a Switch Access Action Assignment Pane that
 * walks a user through the flow of assigning a switch key to a command/action.
 * Note that command and action are used interchangeably with command used
 * internally and action used for user-facing UI.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SwitchAccessCommand} from './switch_access_constants.js';
import {SwitchAccessSubpageBrowserProxy, SwitchAccessSubpageBrowserProxyImpl} from './switch_access_subpage_browser_proxy.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const SettingsSwitchAccessActionAssignmentDialogElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
class SettingsSwitchAccessActionAssignmentDialogElement extends
    SettingsSwitchAccessActionAssignmentDialogElementBase {
  static get is() {
    return 'settings-switch-access-action-assignment-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Set by the main Switch Access subpage to specify which switch action
       * this dialog handles.
       * @type {SwitchAccessCommand}
       */
      action: {
        type: String,
      },

      /**
       * The localized action label.
       * @private {string}
       */
      dialogTitle_: {
        type: String,
        computed: 'getDialogTitleForAction_(action)',
      },
    };
  }

  /** @override */
  constructor() {
    super();

    /** @private {!SwitchAccessSubpageBrowserProxy} */
    this.switchAccessBrowserProxy_ =
        SwitchAccessSubpageBrowserProxyImpl.getInstance();
  }

  ready() {
    super.ready();

    this.addEventListener('exit-pane', this.onPaneExit_);
  }

  /** @private */
  onPaneExit_() {
    this.$.switchAccessActionAssignmentDialog.close();
  }

  /** @private */
  onExitClick_() {
    this.$.switchAccessActionAssignmentDialog.close();
  }

  /**
   * @param {SwitchAccessCommand} action
   * @return {string}
   * @private
   */
  getDialogTitleForAction_(action) {
    return this.i18n(
        'switchAccessActionAssignmentDialogTitle',
        this.getLabelForAction_(action));
  }

  /**
   * @param {SwitchAccessCommand} action
   * @return {string}
   * @private
   */
  getLabelForAction_(action) {
    switch (action) {
      case SwitchAccessCommand.SELECT:
        return this.i18n('assignSelectSwitchLabel');
      case SwitchAccessCommand.NEXT:
        return this.i18n('assignNextSwitchLabel');
      case SwitchAccessCommand.PREVIOUS:
        return this.i18n('assignPreviousSwitchLabel');
      default:
        return '';
    }
  }
}

customElements.define(
    SettingsSwitchAccessActionAssignmentDialogElement.is,
    SettingsSwitchAccessActionAssignmentDialogElement);
