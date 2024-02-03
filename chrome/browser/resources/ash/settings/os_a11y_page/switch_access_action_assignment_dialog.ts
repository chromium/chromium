// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This dialog holds a Switch Access Action Assignment Pane that
 * walks a user through the flow of assigning a switch key to a command/action.
 * Note that command and action are used interchangeably with command used
 * internally and action used for user-facing UI.
 */

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';

import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {assertNotReached} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './switch_access_action_assignment_dialog.html.js';
import {SwitchAccessCommand} from './switch_access_constants.js';
import {SwitchAccessSubpageBrowserProxy, SwitchAccessSubpageBrowserProxyImpl} from './switch_access_subpage_browser_proxy.js';

export interface SettingsSwitchAccessActionAssignmentDialogElement {
  $: {
    switchAccessActionAssignmentDialog: CrDialogElement,
  };
}

const SettingsSwitchAccessActionAssignmentDialogElementBase =
    I18nMixin(PolymerElement);

export class SettingsSwitchAccessActionAssignmentDialogElement extends
    SettingsSwitchAccessActionAssignmentDialogElementBase {
  static get is() {
    return 'settings-switch-access-action-assignment-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Set by the main Switch Access subpage to specify which switch action
       * this dialog handles.
       */
      action: {
        type: String,
      },

      /**
       * The localized action label.
       */
      dialogTitle_: {
        type: String,
        computed: 'getDialogTitleForAction_(action)',
      },
    };
  }

  action: SwitchAccessCommand;
  private dialogTitle_: string;
  private switchAccessBrowserProxy_: SwitchAccessSubpageBrowserProxy;

  constructor() {
    super();

    this.switchAccessBrowserProxy_ =
        SwitchAccessSubpageBrowserProxyImpl.getInstance();
  }

  override ready(): void {
    super.ready();

    this.addEventListener('exit-pane', this.onPaneExit_);
  }

  private onPaneExit_(): void {
    this.$.switchAccessActionAssignmentDialog.close();
  }

  private onExitClick_(): void {
    this.$.switchAccessActionAssignmentDialog.close();
  }

  private getDialogTitleForAction_(action: SwitchAccessCommand): string {
    return this.i18n(
        'switchAccessActionAssignmentDialogTitle',
        this.getLabelForAction_(action));
  }

  private getLabelForAction_(action: SwitchAccessCommand): string {
    switch (action) {
      case SwitchAccessCommand.SELECT:
        return this.i18n('assignSelectSwitchLabel');
      case SwitchAccessCommand.NEXT:
        return this.i18n('assignNextSwitchLabel');
      case SwitchAccessCommand.PREVIOUS:
        return this.i18n('assignPreviousSwitchLabel');
      default:
        assertNotReached();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-switch-access-action-assignment-dialog':
        SettingsSwitchAccessActionAssignmentDialogElement;
  }
}

customElements.define(
    SettingsSwitchAccessActionAssignmentDialogElement.is,
    SettingsSwitchAccessActionAssignmentDialogElement);
