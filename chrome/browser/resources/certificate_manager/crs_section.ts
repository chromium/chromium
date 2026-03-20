// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'crs-section' component is the Chrome Root Store
 * section of the Certificate Management V2 UI.
 */

import './certificate_list.js';

import {getCss as getCrPageHostStyleCss} from '//resources/cr_elements/cr_page_host_style_lit.css.js';
import {getCss as getCrSharedStyleCss} from '//resources/cr_elements/cr_shared_style_lit.css.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import type {CertificateListElement} from './certificate_list.js';
import {getCss as getCertificateManagerStyleCss} from './certificate_manager_style_lit.css.js';
import {getHtml} from './crs_section.html.js';

export interface CrsSectionElement {
  $: {
    crsCerts: CertificateListElement,
  };
}

export class CrsSectionElement extends CrLitElement {
  static get is() {
    return 'crs-section';
  }

  static override get styles() {
    return [
      getCrPageHostStyleCss(),
      getCrSharedStyleCss(),
      getCertificateManagerStyleCss(),
    ];
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      crsLearnMoreUrl_: {type: String},
    };
  }

  protected accessor crsLearnMoreUrl_: string =
      loadTimeData.getString('crsLearnMoreUrl');
}

declare global {
  interface HTMLElementTagNameMap {
    'crs-section': CrsSectionElement;
  }
}

customElements.define(CrsSectionElement.is, CrsSectionElement);
