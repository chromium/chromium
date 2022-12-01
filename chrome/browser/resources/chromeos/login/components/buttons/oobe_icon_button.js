// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 *   @fileoverview
 *   Material design button that shows an icon and displays text.
 *
 *   Example:
 *     <oobe-icon-button icon="close" text-key="offlineLoginCloseBtn">
 *     </oobe-icon-button>
 *    or
 *     <oobe-icon-button icon="close"
 *         label-for-aria="[[i18nDynamic(locale, 'offlineLoginCloseBtn')]]">
 *       <div slot="text">[[i18nDynamic(locale, 'offlineLoginCloseBtn')]]</div>
 *     </oobe-icon-button>
 *
 *   Attributes:
 *     'text-key' - ID of localized string to be used as button text.
 *     1x and 2x icons:
 *       'icon1x' - a name of icon from material design set to show on button.
 *       'icon2x' - a name of icon from material design set to show on button.
 *     'label-for-aria' - accessibility label, override usual behavior
 *                        (string specified by text-key is used as aria-label).
 *                        Elements that use slot="text" must provide
 *                        label-for-aria value.
 *
 */

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icons.css.js';
import '//resources/cr_elements/cr_shared_style.css.js';
import '../common_styles/oobe_common_styles.m.js';
import '../hd_iron_icon.js';
import '../oobe_vars/oobe_custom_vars_css.m.js';

import {html} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeBaseButton} from './oobe_base_button.js';

/** @polymer */
export class OobeIconButton extends OobeBaseButton {
  static get is() {
    return 'oobe-icon-button';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      icon1x: {type: String, observer: 'updateIconVisibility_'},
      icon2x: String,
    };
  }

  updateIconVisibility_() {
    this.$.icon.hidden = (this.icon1x === undefined || this.icon1x.length == 0);
  }
}

customElements.define(OobeIconButton.is, OobeIconButton);
