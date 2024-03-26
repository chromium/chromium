// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Page in eSIM Setup flow that requests user consent to scan for profiles.
 */

import './cellular_setup_icons.html.js';
import '//resources/ash/common/cr_elements/localized_link/localized_link.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/ash/common/cr_elements/cr_shared_style.css.js';
import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import './base_page.js';

import {I18nMixin} from '//resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './profile_discovery_consent_page.html.js';

const ProfileDiscoveryConsentPageElementBase = I18nMixin(PolymerElement);

export class ProfileDiscoveryConsentPageElement extends
    ProfileDiscoveryConsentPageElementBase {
  static get is() {
    return 'profile-discovery-consent-page' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      shouldSkipDiscovery: {
        type: Boolean,
        notify: true,
      },
    };
  }

  shouldSkipDiscovery: boolean;

  private shouldSkipDiscoveryClicked_(e: CustomEvent): void {
    // A place holder href with the value "#" is used to have a compliant link.
    // This prevents the browser from navigating the window to "#"
    e.detail.event.preventDefault();
    e.stopPropagation();
    this.shouldSkipDiscovery = true;
    this.dispatchEvent(new CustomEvent('forward-navigation-requested', {
      bubbles: true, composed: true,
    }));
  }
}

customElements.define(ProfileDiscoveryConsentPageElement.is,
    ProfileDiscoveryConsentPageElement);
