// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/shared/settings/prefs/prefs.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/icons.html.js';
import '../controls/settings_radio_group.js';
import '../privacy_icons.html.js';
import '../privacy_page/collapse_radio_button.js';
import '../settings_shared.css.js';
import './category_setting_exceptions.js';
import './settings_category_default_radio_group.js';
import './site_settings_shared.css.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';

import {getTemplate} from './protected_content_page.html.js';

const PdfDocumentsPageElementBase = PrefsMixin(PolymerElement);

export class PdfDocumentsPageElement extends PdfDocumentsPageElementBase {
  static get is() {
    return 'settings-pdf-documents-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      isGuest_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isGuest');
        },
      },
    };
  }

  declare private isGuest_: boolean;
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-pdf-documents-page': PdfDocumentsPageElement;
  }
}

customElements.define(PdfDocumentsPageElement.is, PdfDocumentsPageElement);
