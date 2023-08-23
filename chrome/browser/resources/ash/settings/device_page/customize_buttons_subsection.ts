// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'customize-buttons-subsection' contains a list of 'customize-button-row'
 * elements that allow users to remap buttons to actions or key combinations.
 */

import '../settings_shared.css.js';
import './customize_button_row.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './customize_buttons_subsection.html.js';
import {ActionChoice, ButtonRemapping} from './input_device_settings_types.js';

const CustomizeButtonsSubsectionElementBase = I18nMixin(PolymerElement);

export class CustomizeButtonsSubsectionElement extends
    CustomizeButtonsSubsectionElementBase {
  static get is() {
    return 'customize-buttons-subsection' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      actionList: {
        type: Array,
      },

      buttonRemappingList: {
        type: Array,
      },
    };
  }

  buttonRemappingList: ButtonRemapping[];
  actionList: ActionChoice[];
}

declare global {
  interface HTMLElementTagNameMap {
    [CustomizeButtonsSubsectionElement.is]: CustomizeButtonsSubsectionElement;
  }
}

customElements.define(
    CustomizeButtonsSubsectionElement.is, CustomizeButtonsSubsectionElement);
