// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'crs-section' component is the Chrome Root Store
 * section of the Certificate Management V2 UI.
 */

import './certificate_list.js';
import './certificate_manager_style.css.js';
import '//resources/cr_elements/cr_shared_style.css.js';
import '//resources/cr_elements/cr_shared_vars.css.js';
import '//resources/cr_elements/cr_page_host_style.css.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {CertificateListElement} from './certificate_list.js';
import {CertificateSource} from './certificate_manager.mojom-webui.js';
import {getTemplate} from './crs_section.html.js';

export interface CrsSectionElement {
  $: {
    crsCerts: CertificateListElement,
  };
}

export class CrsSectionElement extends PolymerElement {
  static get is() {
    return 'crs-section';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      crsLearnMoreUrl_: {
        type: String,
        value: () => loadTimeData.getString('crsLearnMoreUrl'),
      },

      certificateSourceEnum_: {
        type: Object,
        value: CertificateSource,
      },
    };
  }

  declare private crsLearnMoreUrl_: string;
}

declare global {
  interface HTMLElementTagNameMap {
    'crs-section': CrsSectionElement;
  }
}

customElements.define(CrsSectionElement.is, CrsSectionElement);
