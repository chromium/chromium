// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This dialog holds a Switch Access Action Assignment Pane that
 * walks a user through the flow of assigning a switch key to a command/action.
 * Note that command and action are used interchangeably with command used
 * internally and action used for user-facing UI.
 */

Polymer({
  is: 'settings-switch-access-action-assignment-dialog',

  behaviors: [
    I18nBehavior,
  ],

  properties: {
    /**
     * Set by the main Switch Access subpage to specify which switch action this
     * dialog handles.
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
  },

  listeners: {
    'exit-pane': 'onPaneExit_',
  },

  /** @override */
  created() {
    this.switchAccessBrowserProxy_ =
        SwitchAccessSubpageBrowserProxyImpl.getInstance();
  },

  /** @private */
  onPaneExit_() {
    this.$.switchAccessActionAssignmentDialog.close();
  },

  /** @private */
  onExitClick_() {
    this.$.switchAccessActionAssignmentDialog.close();
  },

  /**
   * @param {SwitchAccessCommand} action
   * @return {string}
   * @private
   */
  getDialogTitleForAction_(action) {
    return this.i18n(
        'switchAccessActionAssignmentDialogTitle',
        this.getLabelForAction_(action));
  },

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
  },
});
