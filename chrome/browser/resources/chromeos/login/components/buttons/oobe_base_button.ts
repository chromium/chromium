// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/ash/common/cr_elements/cr_button/cr_button.js';

import type {CrButtonElement} from '//resources/ash/common/cr_elements/cr_button/cr_button.js';
import {assert} from '//resources/js/assert.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeI18nMixin} from '../mixins/oobe_i18n_mixin.js';

export interface OobeBaseButton {
  $: {
    button: CrButtonElement,
  };
}

export const OobeBaseButtonBase = OobeI18nMixin(PolymerElement);

export abstract class OobeBaseButton extends OobeBaseButtonBase {
  static get properties(): PolymerElementProperties {
    return {
      disabled: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      /*
       * The ID of the localized string to be used as button text.
       */
      textKey: {
        type: String,
      },

      labelForAria: {
        type: String,
      },

      labelForAria_: {
        type: String,
        computed: 'computeAriaLabel(labelForAria, locale, textKey)',
      },
    };
  }

  disabled: boolean;
  labelForAria: string;
  private labelForAria_: string;

  override focus(): void {
    this.$.button.focus();
  }

  private computeAriaLabel(
      labelForAria: string, _locale: string, textKey: string): string {
    if (labelForAria) {
      return labelForAria;
    }
    return (!textKey) ? '' : this.i18n(textKey);
  }

  private onClick(): void {
    // Just checking here. The event is propagated further.
    assert(!this.disabled);
  }
}
