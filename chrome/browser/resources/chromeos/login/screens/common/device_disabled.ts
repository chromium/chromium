// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for Device Disabled message screen.
 */

import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_icons.html.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';

import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {OobeDialogHostBehavior, OobeDialogHostBehaviorInterface} from '../../components/behaviors/oobe_dialog_host_behavior.js';
import {OobeI18nMixin, OobeI18nMixinInterface} from '../../components/mixins/oobe_i18n_mixin.js';
import {OobeUiState} from '../../components/display_manager_types.js';

import {getTemplate} from './device_disabled.html.js';


const DeviceDisabledElementBase =
    mixinBehaviors(
        [LoginScreenBehavior, OobeDialogHostBehavior],
        OobeI18nMixin(PolymerElement)) as {
      new (): PolymerElement & OobeI18nMixinInterface &
          LoginScreenBehaviorInterface & OobeDialogHostBehaviorInterface,
    };


interface DeviceDisabledScreenData {
  serial: string;
  domain: string;
  message: string;
}

export class DeviceDisabled extends DeviceDisabledElementBase {
  static get is() {
    return 'device-disabled-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       * The serial number of the device.
       */
      serial: {
        type: String,
        value: '',
      },

      /**
       * The domain that owns the device (can be empty).
       */
      enrollmentDomain: {
        type: String,
        value: '',
      },

      /**
       * Admin message (external data, non-html-safe).
       */
      message: {
        type: String,
        value: '',
      },
    };
  }

  private serial: string;
  private enrollmentDomain: string;
  private message: string;

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('DeviceDisabledScreen');
  }

  /** @override */
  override get EXTERNAL_API(): string[] {
    return ['setMessage'];
  }

  /** Initial UI State for screen */
  // eslint-disable-next-line @typescript-eslint/naming-convention
  override getOobeUIInitialState(): OobeUiState {
    return OobeUiState.BLOCKING;
  }

  override get defaultControl(): HTMLElement|null {
    return this.shadowRoot!.querySelector('#dialog');
  }

  /**
   * Event handler that is invoked just before the frame is shown.
   * data Screen init payload.
   */
  override onBeforeShow(data: DeviceDisabledScreenData): void {
    if ('serial' in data) {
      this.serial = data.serial;
    }
    if ('domain' in data) {
      this.enrollmentDomain = data.domain;
    }
    if ('message' in data) {
      this.message = data.message;
    }
  }

  /**
   * Sets the message to be shown to the user.
   */
  setMessage(message: string): void {
    this.message = message;
  }

  /**
   * Updates the explanation shown to the user. The explanation contains the
   * device serial number and may contain the domain the device is enrolled to,
   * if that information is available.
   * locale The i18n locale.
   * serial The device serial number.
   * domain The enrollment domain.
   * return The internationalized explanation.
   */
  private disabledText(locale: string, serial: string, domain: string):
      TrustedHTML {
    if (domain) {
      return this.i18nAdvancedDynamic(
          locale, 'deviceDisabledExplanationWithDomain',
          {substitutions: [serial, domain]});
    }
    return this.i18nAdvancedDynamic(
        locale, 'deviceDisabledExplanationWithoutDomain',
        {substitutions: [serial]});
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [DeviceDisabled.is]: DeviceDisabled;
  }
}

customElements.define(DeviceDisabled.is, DeviceDisabled);
