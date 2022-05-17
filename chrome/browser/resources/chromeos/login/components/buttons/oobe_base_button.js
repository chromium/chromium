// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #import {PolymerElement, mixinBehaviors} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../behaviors/oobe_i18n_behavior.m.js';
// #import {assert} from '//resources/js/assert.m.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {OobeI18nBehaviorInterface}
 */
const OobeBaseButtonBase = Polymer.mixinBehaviors(
  [OobeI18nBehavior], Polymer.Element);

/**
 * @polymer
 */
/* #export */ class OobeBaseButton extends OobeBaseButtonBase {

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