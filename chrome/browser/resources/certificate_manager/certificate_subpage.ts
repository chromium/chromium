// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'certificate-subpage' component is designed to show a
 * subpage. This subpage contains:
 *
 *   - header text
 *   - one or more lists of certs
 *   - a back button for navigating back to the previous page
 *
 * This component is used in the new Certificate Management UI in
 * ./certificate_manager.ts.
 */

import '/strings.m.js';
import './certificate_list.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_icons.css.js';
import '//resources/cr_elements/cr_shared_style.css.js';

import {I18nMixin} from '//resources/cr_elements/i18n_mixin.js';
import {focusWithoutInk} from '//resources/js/focus_without_ink.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {CertificateSource} from './certificate_manager.mojom-webui.js';
import {getTemplate} from './certificate_subpage.html.js';
import {Page, Router} from './navigation.js';

export interface CertificateSubpageElement {
  $: {
    backButton: HTMLElement,
  };
}

export class SubpageCertificateList {
  headerText: string;
  certSource: CertificateSource;
  certMetadataEditable?: boolean;
  hideExport?: boolean;
  showImport?: boolean;
  showImportAndBind?: boolean;
  hideIfEmpty?: boolean;
  hideHeader?: boolean;
}

const CertificateSubpageElementBase = I18nMixin(PolymerElement);

export class CertificateSubpageElement extends CertificateSubpageElementBase {
  static get is() {
    return 'certificate-subpage';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      subpageTitle: String,

      subpageCertLists: {
        type: Array,
        value: () => [],
      },

      navigateBackTarget: Page,
    };
  }

  declare subpageTitle: string;
  declare subpageCertLists: SubpageCertificateList[];
  declare navigateBackTarget: Page;
  navigateBackSource: Page;

  // Sets initial keyboard focus of the subpage. Assumes that subpage elements
  // are visible.
  setInitialFocus() {
    focusWithoutInk(this.$.backButton);
  }

  private onBackButtonClick_(e: Event) {
    e.preventDefault();
    Router.getInstance().navigateTo(this.navigateBackTarget);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'certificate-subpage': CertificateSubpageElement;
  }
}

customElements.define(CertificateSubpageElement.is, CertificateSubpageElement);
