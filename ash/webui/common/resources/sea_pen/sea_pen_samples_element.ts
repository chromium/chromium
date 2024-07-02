// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A polymer element that displays a list of freeform prompt
 * samples.
 */

import 'chrome://resources/ash/common/personalization/common.css.js';
import 'chrome://resources/ash/common/personalization/wallpaper.css.js';
import 'chrome://resources/ash/common/sea_pen/sea_pen.css.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SEA_PEN_SAMPLES, SeaPenSamplePrompt} from './constants.js';
import {getTemplate} from './sea_pen_samples_element.html.js';

const SeaPenSamplesElementBase = I18nMixin(PolymerElement);

export class SeaPenSamplesElement extends SeaPenSamplesElementBase {
  static get is() {
    return 'sea-pen-samples';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      samples: {
        type: Array,
        value: SEA_PEN_SAMPLES,
      },
    };
  }

  private samples: SeaPenSamplePrompt[];
}

customElements.define(SeaPenSamplesElement.is, SeaPenSamplesElement);
