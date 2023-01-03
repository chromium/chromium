// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Text is blue, background is white.
 * Material design square "Next ->" button.
 *
 * Example:
 *    <oobe-next-button on-click="handleOnClick_"></oobe-next-button>
 *
 *    The content of button can be overridden from the default ("Next")
 *    by specifying a text-key property or by setting the text directly
 *    via the "text" slot:
 *
 *    <oobe-next-button on-click="handleOnClick_" text-key="continueButton">
 *    </oobe-next-button>
 *
 *    or
 *
 *    <oobe-next-button on-click="handleOnClick_"
 *        label-for-aria="[[i18nDynamic(locale, 'continueButton')]]">
 *      <div slot="text">[[i18nDynamic(locale, 'continueButton')]]</div>
 *   </oobe-next-button>
 *
 *  Attributes:
 *    'disabled' - button is disabled when the attribute is set.
 *    (See crbug.com/900640)
 *    'text-key' - ID of localized string to be used as button text.
 *    'label-for-aria' - accessibility label, override usual behavior
 *                       (string specified by text-key is used as aria-label).
 *                       Elements that use slot="text" must provide
 *                       label-for-aria value.
 */


import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icons.css.js';
import '//resources/cr_elements/cr_shared_style.css.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../common_styles/oobe_common_styles.css.js';
import '../oobe_vars/oobe_custom_vars.css.js';
import '../oobe_icons.html.js';

import {html} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeBaseButton} from './oobe_base_button.js';


/** @polymer */
export class OobeNextButton extends OobeBaseButton {
  static get is() {
    return 'oobe-next-button';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /* The ID of the localized string to be used as button text.
       */
      textKey: {
        type: String,
        value: 'next',
      },
    };
  }
}

customElements.define(OobeNextButton.is, OobeNextButton);
