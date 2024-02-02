// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 *   Material design square "<- Back" button.
 *   Text is blue, background is white.
 *
 *   Example:
 *    <oobe-back-button on-click="handleOnClick_"></oobe-back-button>
 *
 *     The content of button can be overridden from the default ("Back")
 *     by specifying a text-key property or by setting the text directly
 *     via the "text" slot:
 *
 *    <oobe-back-button on-click="handleOnClick_" text-key="continueButton">
 *     </oobe-back-button>
 *
 *     or
 *
 *     <oobe-back-button on-click="handleOnClick_"
 *         label-for-aria="[[i18nDynamic(locale, 'continueButton')]]">
 *       <div slot="text">[[i18nDynamic(locale, 'continueButton')]]</div>
 *     </oobe-back-button>
 *
 *  Attributes:
 *     'disabled' - button is disabled when the attribute is set.
 *     (See crbug.com/900640)
 *     'text-key' - ID of localized string to be used as button text.
 *     'label-for-aria' - accessibility label, override usual behavior
 *                        (string specified by text-key is used as aria-label).
 *                        Elements that use slot="text" must provide
 * label-for-aria value.
 */

import '//resources/ash/common/cr_elements/cros_color_overrides.css.js';
import '//resources/ash/common/cr_elements/cr_button/cr_button.js';
import '//resources/ash/common/cr_elements/cr_icons.css.js';
import '//resources/ash/common/cr_elements/cr_shared_style.css.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../common_styles/oobe_common_styles.css.js';
import '../oobe_vars/oobe_custom_vars.css.js';
import '../oobe_icons.html.js';

import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {getTemplate} from './oobe_back_button.html.js';

import {OobeBaseButton} from './oobe_base_button.js';

export class OobeBackButton extends OobeBaseButton {
  static get is() {
    return 'oobe-back-button' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static override get properties(): PolymerElementProperties {
    return {
      /* The ID of the localized string to be used as button text.
       */
      textKey: {
        type: String,
        value: 'back',
      },
    };
  }

  textKey: string;
}

declare global {
  interface HTMLElementTagNameMap {
    [OobeBackButton.is]: OobeBackButton;
  }
}

customElements.define(OobeBackButton.is, OobeBackButton);
