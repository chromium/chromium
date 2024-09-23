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

import '//resources/ash/common/cr_elements/cros_color_overrides.css.js';
import '//resources/ash/common/cr_elements/cr_button/cr_button.js';
import '//resources/ash/common/cr_elements/cr_icons.css.js';
import '//resources/ash/common/cr_elements/cr_shared_style.css.js';
import '../common_styles/oobe_common_styles.css.js';
import '../oobe_vars/oobe_custom_vars.css.js';
import '../hd_iron_icon.js';

import type {CrButtonElement} from '//resources/ash/common/cr_elements/cr_button/cr_button.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';

import type {HdIronIcon} from '../hd_iron_icon.js';

import {OobeBaseButton} from './oobe_base_button.js';
import {getTemplate} from './oobe_icon_button.html.js';

export interface OobeIconButton extends OobeBaseButton {
  $: {
    button: CrButtonElement,
    icon: HdIronIcon,
  };
}

export class OobeIconButton extends OobeBaseButton {
  static get is() {
    return 'oobe-icon-button' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static override get properties(): PolymerElementProperties {
    return {
      icon1x: {type: String, observer: 'updateIconVisibility'},
      icon2x: String,
    };
  }

  icon1x: string;
  icon2x: string;

  private updateIconVisibility(): void {
    this.$.icon.hidden = (this.icon1x === undefined || this.icon1x.length === 0);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [OobeIconButton.is]: OobeIconButton;
  }
}

customElements.define(OobeIconButton.is, OobeIconButton);
