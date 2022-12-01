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
import './common_styles/oobe_common_styles.m.js';

import {html, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @polymer
 */
class HdIronIcon extends PolymerElement {
  static get is() {
    return 'hd-iron-icon';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      icon1x: String,
      icon2x: String,
      src1x: String,
      src2x: String,
    };
  }
}

customElements.define(HdIronIcon.is, HdIronIcon);
