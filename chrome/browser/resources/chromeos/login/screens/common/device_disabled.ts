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
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeUiState} from '../../components/display_manager_types.js';
import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {OobeDialogHostMixin} from '../../components/mixins/oobe_dialog_host_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';

import {getTemplate} from './device_disabled.html.js';


const DeviceDisabledElementBase =
    OobeDialogHostMixin(LoginScreenMixin(OobeI18nMixin(PolymerElement)));


interface DeviceDisabledScreenData {
  serial: string;
  domain: string;
  message: string;
  deviceRestrictionScheduleEnabled: boolean;
  deviceName: string;
  restrictionScheduleEndDay: string;
  restrictionScheduleEndTime: string;
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

      /**
       * Flag indicating if the device was disabled because it is in restricted
       * schedule.
       */
      deviceRestrictionScheduleEnabled: {
        type: Boolean,
        value: false,
      },

      /**
       * The name of the ChromeOS device.
       */
      deviceName: {
        type: String,
        value: '',
      },

      /**
       * The day at which the restriction schedule ends.
       */
      restrictionScheduleEndDay: {
        type: String,
        value: '',
      },

      /**
       * The time at which the restriction schedule ends.
       */
      restrictionScheduleEndTime: {
        type: String,
        value: '',
      },
    };
  }

  private serial: string;
  private enrollmentDomain: string;
  private message: string;
  private deviceRestrictionScheduleEnabled: boolean;
  private deviceName: string;
  private restrictionScheduleEndDay: string;
  private restrictionScheduleEndTime: string;

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
    super.onBeforeShow(data);
    if ('serial' in data) {
      this.serial = data.serial;
    }
    if ('domain' in data) {
      this.enrollmentDomain = data.domain;
    }
    if ('message' in data) {
      this.message = data.message;
    }
    if ('deviceRestrictionScheduleEnabled' in data) {
      this.deviceRestrictionScheduleEnabled =
          data.deviceRestrictionScheduleEnabled;
    }
    if ('deviceName' in data) {
      this.deviceName = data.deviceName;
    }
    if ('restrictionScheduleEndDay' in data) {
      this.restrictionScheduleEndDay = data.restrictionScheduleEndDay;
    }
    if ('restrictionScheduleEndTime' in data) {
      this.restrictionScheduleEndTime = data.restrictionScheduleEndTime;
    }
  }

  /**
   * Sets the message to be shown to the user.
   */
  setMessage(message: string): void {
    this.message = message;
  }

  /**
   * Updates the heading for the explanation shown to the user.
   * locale The i18n locale.
   * deviceRestrictionScheduleEnabled Flag indicating if the device was disabled
   *     because the device is in restriction schedule.
   * return The internationalized heading for the explanation.
   */
  private disabledHeading(
      locale: string, deviceRestrictionScheduleEnabled: boolean): TrustedHTML {
    if (deviceRestrictionScheduleEnabled) {
      return this.i18nAdvancedDynamic(
          locale, 'deviceDisabledHeadingRestrictionSchedule');
    }
    return this.i18nAdvancedDynamic(locale, 'deviceDisabledHeading');
  }

  /**
   * Updates the explanation shown to the user. The explanation contains the
   * device serial number and may contain the domain the device is enrolled to,
   * if that information is available.
   * If `deviceRestrictionScheduleEnabled` is true, a custom explanation about
   * device restriction schedule will be used.
   * locale The i18n locale.
   * serial The device serial number.
   * domain The enrollment domain.
   * deviceRestrictionScheduleEnabled flag indicating if the device was disabled
   *     because the device is in restriction schedule.
   * deviceName The name of the ChromeOS device.
   * restrictionScheduleEndDay The day at which the restriction schedule ends.
   * restrictionScheduleEndTime The time at which the restriction schedule ends.
   * return The internationalized explanation.
   */
  private disabledText(
      locale: string, serial: string, domain: string,
      deviceRestrictionScheduleEnabled: boolean, deviceName: string,
      restrictionScheduleEndDay: string,
      restrictionScheduleEndTime: string): TrustedHTML {
    if (deviceRestrictionScheduleEnabled) {
      return this.i18nAdvancedDynamic(
          locale, 'deviceDisabledExplanationRestrictionSchedule', {
            substitutions: [
              domain,
              deviceName,
              restrictionScheduleEndDay,
              restrictionScheduleEndTime,
            ],
          });
    }
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
