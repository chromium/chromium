// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'site-details-permission-device-entry' shows a single device for a given
 * chooser exception.
 */
import '/shared/settings/controls/cr_policy_pref_indicator.js';
import '../settings_shared.css.js';

import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './site_details_permission_device_entry.html.js';
import type {ChooserException, SiteException} from './site_settings_prefs_browser_proxy.js';
import {SiteSettingsPrefsBrowserProxyImpl} from './site_settings_prefs_browser_proxy.js';

export interface SiteDetailsPermissionDeviceEntryElement {
  $: {
    resetSite: HTMLElement,
  };
}

export class SiteDetailsPermissionDeviceEntryElement extends PolymerElement {
  static get is() {
    return 'site-details-permission-device-entry';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The chooser exception to display in the widget.
       */
      exception: Object,
    };
  }

  exception: ChooserException;

  /**
   * Get the SiteException that is enforced from |this.exception.sites| if any.
   */
  private getPolicyPref_(): SiteException|null {
    return this.exception.sites.find(site => {
      return site.enforcement === chrome.settingsPrivate.Enforcement.ENFORCED &&
          !!site.controlledBy;
    }) ||
        null;
  }

  private shouldShowPolicyPrefIndicator_(): boolean {
    return !!this.getPolicyPref_();
  }

  private onResetButtonClick_() {
    assert(this.exception.sites.length > 0);
    SiteSettingsPrefsBrowserProxyImpl.getInstance()
        .resetChooserExceptionForSite(
            this.exception.chooserType, this.exception.sites[0].origin,
            this.exception.object);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'site-details-permission-device-entry':
        SiteDetailsPermissionDeviceEntryElement;
  }
}

customElements.define(
    SiteDetailsPermissionDeviceEntryElement.is,
    SiteDetailsPermissionDeviceEntryElement);
