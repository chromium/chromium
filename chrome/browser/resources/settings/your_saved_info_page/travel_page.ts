// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-travel-page', is a subpage of the "Your saved info"
 * section. It manages the user's autofill data for traveling. Users can add,
 * edit, or delete their saved document details, as well as opt out of the
 * autofill functionality entirely.
 */

import '/shared/settings/prefs/prefs.js';
import '../autofill_page/autofill_ai_entries_list.js';
import '../autofill_page/your_saved_info_shared.css.js';
import '../settings_page/settings_subpage.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {EntityTypeName} from '../autofill_ai_enums.mojom-webui.js';
import {SettingsViewMixin} from '../settings_page/settings_view_mixin.js';

import {getTemplate} from './travel_page.html.js';

const SettingsTravelPageElementBase =
    SettingsViewMixin(PrefsMixin(PolymerElement));

export class SettingsTravelPageElement extends SettingsTravelPageElementBase {
  static get is() {
    return 'settings-travel-page';
  }

  static get template() {
    return getTemplate();
  }

  private getAllowedEntityTypes_(): Set<EntityTypeName> {
    return new Set([
      EntityTypeName.kFlightReservation,
      EntityTypeName.kKnownTravelerNumber,
      EntityTypeName.kRedressNumber,
      EntityTypeName.kVehicle,
    ]);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-travel-page': SettingsTravelPageElement;
  }
}

customElements.define(SettingsTravelPageElement.is, SettingsTravelPageElement);
