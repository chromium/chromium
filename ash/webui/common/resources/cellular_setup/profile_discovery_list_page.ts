// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Page in eSIM Setup flow that displays a choice of available eSIM Profiles.
 */

import '//resources/ash/common/cr_elements/cr_shared_style.css.js';
import '//resources/polymer/v3_0/iron-list/iron-list.js';
import '//resources/ash/common/cr_elements/localized_link/localized_link.js';
import './base_page.js';
import './profile_discovery_list_item.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {ESimProfileProperties} from 'chrome://resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-webui.js';

import {getTemplate} from './profile_discovery_list_page.html.js';

const ProfileDiscoveryListPageElementBase = I18nMixin(PolymerElement);

export class ProfileDiscoveryListPageElement extends
    ProfileDiscoveryListPageElementBase {
  static get is() {
    return 'profile-discovery-list-page' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      pendingProfileProperties: Array,

      selectedProfileProperties: {
        type: Object,
        notify: true,
      },
    };
  }

  pendingProfileProperties: ESimProfileProperties[];
  selectedProfileProperties: ESimProfileProperties|null;

  attemptToFocusOnFirstProfile(): boolean {
    if (!this.pendingProfileProperties ||
        this.pendingProfileProperties.length === 0) {
      return false;
    }

    const items =
        this.shadowRoot!.querySelectorAll('profile-discovery-list-item');
    const item = items[0] as HTMLElement;
    assert(items.length > 0);
    item.focus();
    item.setAttribute('selected', 'true');
    return true;
  }

  private isProfilePropertiesSelected_(profileProperties:
                                           ESimProfileProperties): boolean {
    return this.selectedProfileProperties === profileProperties;
  }

  private enterManuallyClicked_(e: CustomEvent): void {
    e.detail.event.preventDefault();
    e.stopPropagation();
    this.selectedProfileProperties = null;
    this.dispatchEvent(new CustomEvent('forward-navigation-requested', {
      bubbles: true,
      composed: true,
    }));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [ProfileDiscoveryListPageElement.is]: ProfileDiscoveryListPageElement;
  }
}

customElements.define(
    ProfileDiscoveryListPageElement.is, ProfileDiscoveryListPageElement);
