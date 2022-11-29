// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


import '//resources/cr_elements/chromeos/cros_color_overrides.css.js';
import '//resources/cr_elements/icons.html.js';

import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeI18nBehavior, OobeI18nBehaviorInterface} from './behaviors/oobe_i18n_behavior.js';


/**
 *  A single screen item.
 * @typedef {{
 *   icon: String,
 *   title: String,
 *   screenID: String,
 *   selected: Boolean,
 * }}
 */
export let ScreenItem;

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {OobeI18nBehaviorInterface}
 */
const OobeScreensListBase = mixinBehaviors([OobeI18nBehavior], PolymerElement);

/**
 * @polymer
 */
export class OobeScreensList extends OobeScreensListBase {
  static get is() {
    return 'oobe-screens-list';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * List of screens to display.
       * @type {!Array<ScreenItem>}
       */
      screensList: {
        type: Array,
        value: [],
      },
      /**
       * Number of screens selected.
       */
      screensSelected: {
        type: Number,
        value: 0,
        notify: true,
      },
    };
  }

  onclick_(e) {
    var selected = e.model.item.selected;
    e.model.item.selected = !selected;
    e.currentTarget.setAttribute('checked', !selected);
    if (!selected) {
      this.screensSelected++;
    } else {
      this.screensSelected--;
    }
  }
}

customElements.define(OobeScreensList.is, OobeScreensList);
