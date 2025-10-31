// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-identity-docs-page', is a subpage of the "Your saved
 * info" section. It manages the user's autofill data for identity documents.
 * Users can add, edit, or delete their saved document details, as well as opt
 * out of the autofill functionality entirely.
 */

import '/shared/settings/prefs/prefs.js';
import '../autofill_page/autofill_ai_entries_list.js';
import '../autofill_page/your_saved_info_shared.css.js';
import '../settings_page/settings_subpage.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {EntityTypeName} from '../autofill_ai_enums.mojom-webui.js';
import {SettingsViewMixin} from '../settings_page/settings_view_mixin.js';

import {getTemplate} from './identity_docs_page.html.js';

const SettingsIdentityDocsPageElementBase =
    SettingsViewMixin(PrefsMixin(PolymerElement));

export class SettingsIdentityDocsPageElement extends
    SettingsIdentityDocsPageElementBase {
  static get is() {
    return 'settings-identity-docs-page';
  }

  static get template() {
    return getTemplate();
  }

  private getAllowedEntityTypes_(): Set<EntityTypeName> {
    return new Set([
      EntityTypeName.kDriversLicense,
      EntityTypeName.kNationalIdCard,
      EntityTypeName.kPassport,
    ]);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-identity-docs-page': SettingsIdentityDocsPageElement;
  }
}

customElements.define(
    SettingsIdentityDocsPageElement.is, SettingsIdentityDocsPageElement);
