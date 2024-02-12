// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview <iron-icon> that automatically displays one of the two icons
 * depending on display resolution,
 *
 * Example:
 *    <hd-iron-icon icon1x="icons:wifi-1x"
 * icon2x="icons:wifi-2x"></hd-iron-icon>
 *
 *  Attributes:
 *    1x and 2x icons:
 *      'icon1x' - a name of icon from material design set to show on button.
 *      'icon2x' - a name of icon from material design set to show on button.
 *   1x and 2x sources:
 *     'src1x' - A direct source to a file. For example, a SVG file.
 *     'src2x' - A direct source to a file. For example, a SVG file.
 */

import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import './common_styles/oobe_common_styles.css.js';

import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './hd_iron_icon.html.js';

export class HdIronIcon extends PolymerElement {
  static get is() {
    return 'hd-iron-icon' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }


  static get properties(): PolymerElementProperties {
    return {
      icon1x: String,
      icon2x: String,
      src1x: String,
      src2x: String,
    };
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [HdIronIcon.is]: HdIronIcon;
  }
}

customElements.define(HdIronIcon.is, HdIronIcon);
