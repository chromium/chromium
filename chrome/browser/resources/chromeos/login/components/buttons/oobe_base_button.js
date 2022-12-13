// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/ash/common/assert.js';
import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../behaviors/oobe_i18n_behavior.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {OobeI18nBehaviorInterface}
 */
const OobeBaseButtonBase = mixinBehaviors([OobeI18nBehavior], PolymerElement);

/**
 * @polymer
 */
export class OobeBaseButton extends OobeBaseButtonBase {
  static get properties() {
    return {
      disabled: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      /* The ID of the localized string to be used as button text.
       */
      textKey: {
        type: String,
      },

      labelForAria: {
        type: String,
      },

      labelForAria_: {
        type: String,
        computed: 'ariaLabel_(labelForAria, locale, textKey)',
      },
    };
  }

  focus() {
    this.$.button.focus();
  }

  ariaLabel_(labelForAria, locale, textKey) {
    if (labelForAria) {
      return labelForAria;
    }
    return (!textKey) ? '' : this.i18n(textKey);
  }

  onClick_(e) {
    // Just checking here. The event is propagated further.
    assert(!this.disabled);
  }
}