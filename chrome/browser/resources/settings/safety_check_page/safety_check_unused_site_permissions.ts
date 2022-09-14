// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-safety-unused-site-permissions' is the settings page containing the
 * safety check unused site permissions module showing the unused sites that has
 * some granted permissions.
 */

import './safety_check_child.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {routes} from '../route.js';
import {Router} from '../router.js';

import {SafetyCheckIconStatus, SettingsSafetyCheckChildElement} from './safety_check_child.js';
import {getTemplate} from './safety_check_unused_site_permissions.html.js';

export interface SettingsSafetyCheckUnusedSitePermissionsElement {
  $: {
    'safetyCheckChild': SettingsSafetyCheckChildElement,
  };
}

export class SettingsSafetyCheckUnusedSitePermissionsElement extends
    PolymerElement {
  static get is() {
    return 'settings-safety-check-unused-site-permissions';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      iconStatus_: {
        type: SafetyCheckIconStatus,
        value() {
          return SafetyCheckIconStatus.UNUSED_SITE_PERMISSIONS;
        },
      },
    };
  }

  private iconStatus_: SafetyCheckIconStatus;

  private onButtonClick_() {
    Router.getInstance().navigateTo(
        routes.SITE_SETTINGS, /* dynamicParams= */ undefined,
        /* removeSearch= */ true);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-safety-check-unused-site-permissions':
        SettingsSafetyCheckUnusedSitePermissionsElement;
  }
}

customElements.define(
    SettingsSafetyCheckUnusedSitePermissionsElement.is,
    SettingsSafetyCheckUnusedSitePermissionsElement);
