// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';

import type {Authenticator} from '/gaia_auth_host/authenticator.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './gaia_action_buttons.html.js';

interface ActionButtonsData {
  primaryActionButtonLabel: string;
  primaryActionButtonEnabled: boolean;
  secondaryActionButtonLabel: string;
  secondaryActionButtonEnabled: boolean;
}

/**
 * Event listeners for the events triggered by the authenticator.
 */
const authenticatorEventListeners: Array<{event: string, field: string}> = [
  {event: 'setPrimaryActionLabel', field: 'primaryActionButtonLabel'},
  {event: 'setPrimaryActionEnabled', field: 'primaryActionButtonEnabled'},
  {event: 'setSecondaryActionLabel', field: 'secondaryActionButtonLabel'},
  {event: 'setSecondaryActionEnabled', field: 'secondaryActionButtonEnabled'},
];

export class GaiaActionButtonsElement extends PolymerElement {
  static get is() {
    return 'gaia-action-buttons';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The authenticator instance.
       * type {?Authenticator} (omitting @ on purpose since it breaks TS)
       */
      authenticator: {
        type: Object,
        observer: 'authenticatorChanged_',
      },

      roundedButton: {
        type: Boolean,
        value: false,
      },

      actionButtonClasses_: {
        type: String,
        computed: 'getActionButtonClasses_(roundedButton)',
      },

      secondaryButtonClasses_: {
        type: String,
        computed: 'getSecondaryButtonClasses_(roundedButton)',
      },

      /**
       * Controls label and availability on the action buttons.
       */
      data_: {
        type: Object,
        value() {
          return {
            primaryActionButtonLabel: '',
            primaryActionButtonEnabled: true,
            secondaryActionButtonLabel: '',
            secondaryActionButtonEnabled: true,
          };
        },
      },
    };
  }

  declare authenticator: Authenticator;
  declare roundedButton: boolean;
  declare private actionButtonClasses_: string;
  declare private secondaryButtonClasses_: string;
  declare private data_: ActionButtonsData;

  private authenticatorChanged_() {
    if (this.authenticator) {
      this.addAuthenticatorListeners_();
    }
  }

  private addAuthenticatorListeners_() {
    authenticatorEventListeners.forEach(listenParams => {
      this.authenticator.addEventListener(listenParams.event, e => {
        this.set(
            `data_.${listenParams.field}`, (e as CustomEvent<string>).detail);
      });
    });
    this.authenticator.addEventListener(
        'setAllActionsEnabled',
        e => this.onSetAllActionsEnabled_(e as CustomEvent<boolean>));
  }

  /**
   * Invoked when the auth host emits 'setAllActionsEnabled' event
   */
  private onSetAllActionsEnabled_(e: CustomEvent<boolean>) {
    this.set('data_.primaryActionButtonEnabled', e.detail);
    this.set('data_.secondaryActionButtonEnabled', e.detail);
  }

  /**
   * Handles clicks on "PrimaryAction" button.
   */
  private onPrimaryActionButtonClicked_() {
    this.authenticator.sendMessageToWebview('primaryActionHit');
    this.focusWebview_();
  }

  /**
   * Handles clicks on "SecondaryAction" button.
   */
  private onSecondaryActionButtonClicked_() {
    this.authenticator.sendMessageToWebview('secondaryActionHit');
    this.focusWebview_();
  }

  private focusWebview_() {
    this.dispatchEvent(new CustomEvent(
        'set-focus-to-webview', {bubbles: true, composed: true}));
  }

  private getActionButtonClasses_(roundedButton: boolean): string {
    const cssClasses = ['action-button'];
    if (roundedButton) {
      cssClasses.push('rounded-button');
    }
    return cssClasses.join(' ');
  }

  private getSecondaryButtonClasses_(roundedButton: boolean): string {
    const cssClasses = ['secondary-button'];
    if (roundedButton) {
      cssClasses.push('rounded-button');
    }
    return cssClasses.join(' ');
  }

  setAuthenticatorForTest(authenticator: Authenticator) {
    this.authenticator = authenticator;
    this.addAuthenticatorListeners_();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'gaia-action-buttons': GaiaActionButtonsElement;
  }
}

customElements.define(GaiaActionButtonsElement.is, GaiaActionButtonsElement);
