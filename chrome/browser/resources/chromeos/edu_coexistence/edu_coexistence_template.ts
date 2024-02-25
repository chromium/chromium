// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './common.css.js';

import {CrScrollableMixin} from 'chrome://resources/ash/common/cr_elements/cr_scrollable_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './edu_coexistence_template.html.js';

const EduCoexistenceTemplateBase = CrScrollableMixin(PolymerElement);

export class EduCoexistenceTemplate extends EduCoexistenceTemplateBase {
  static get is() {
    return 'edu-coexistence-template' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Indicates whether the footer/button div should be shown.
       */
      showButtonFooter: {
        type: Boolean,
        value: false,
      },
    };
  }

  showButtonFooter: boolean;

  updateButtonFooterVisibility(visible: boolean) {
    this.showButtonFooter = visible;
  }

  getContentContainer(): HTMLElement {
    const contentContainer =
        this.shadowRoot!.querySelector<HTMLElement>('.content-container');
    return contentContainer!;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [EduCoexistenceTemplate.is]: EduCoexistenceTemplate;
  }
}

customElements.define(EduCoexistenceTemplate.is, EduCoexistenceTemplate);
