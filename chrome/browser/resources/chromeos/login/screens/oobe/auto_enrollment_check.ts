// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Oobe Auto-enrollment check screen implementation.
 */

import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/dialogs/oobe_loading_dialog.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {OobeDialogHostMixin} from '../../components/mixins/oobe_dialog_host_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';

import {getTemplate} from './auto_enrollment_check.html.js';


export const AutoEnrollmentCheckElementBase =
    OobeDialogHostMixin(LoginScreenMixin(OobeI18nMixin(PolymerElement)));

export class AutoEnrollmentCheckElement extends AutoEnrollmentCheckElementBase {
  static get is() {
    return 'auto-enrollment-check-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       * Whether to show get device ready title.
       */
      isOobeSoftwareUpdateEnabled: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isOobeSoftwareUpdateEnabled');
        },
      },
    };
  }

  private isOobeSoftwareUpdateEnabled: boolean;

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('AutoEnrollmentCheckScreen');
  }

  private getLoadingTitle(): string {
    if (this.isOobeSoftwareUpdateEnabled) {
      return 'gettingDeviceReadyTitle';
    }
    return 'autoEnrollmentCheckMessage';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [AutoEnrollmentCheckElement.is]: AutoEnrollmentCheckElement;
  }
}

customElements.define(
    AutoEnrollmentCheckElement.is, AutoEnrollmentCheckElement);
