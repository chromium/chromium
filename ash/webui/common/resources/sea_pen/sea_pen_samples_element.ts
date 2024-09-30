// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A polymer element that displays a list of freeform prompt
 * samples.
 */

import 'chrome://resources/ash/common/personalization/common.css.js';
import 'chrome://resources/ash/common/personalization/cros_button_style.css.js';
import 'chrome://resources/ash/common/personalization/personalization_shared_icons.html.js';
import 'chrome://resources/ash/common/personalization/wallpaper.css.js';
import 'chrome://resources/ash/common/sea_pen/sea_pen.css.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {WallpaperGridItemSelectedEvent} from 'chrome://resources/ash/common/personalization/wallpaper_grid_item_element.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SeaPenSamplePrompt} from './constants.js';
import {logSamplePromptClicked} from './sea_pen_metrics_logger.js';
import {getTemplate} from './sea_pen_samples_element.html.js';

const SeaPenSamplesElementBase = I18nMixin(PolymerElement);

export class SeaPenSampleSelectedEvent extends CustomEvent<string> {
  static readonly EVENT_NAME = 'sea-pen-sample-selected';

  constructor(prompt: string) {
    super(
        SeaPenSampleSelectedEvent.EVENT_NAME,
        {
          bubbles: true,
          composed: true,
          detail: prompt,
        },
    );
  }
}

declare global {
  interface HTMLElementEventMap {
    [SeaPenSampleSelectedEvent.EVENT_NAME]: SeaPenSampleSelectedEvent;
  }
}

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
      },
    };
  }

  private samples: SeaPenSamplePrompt[];

  private onClickSample_(e: WallpaperGridItemSelectedEvent&
                         {model: {sample: SeaPenSamplePrompt}}) {
    logSamplePromptClicked(e.model.sample.id);
    this.dispatchEvent(new SeaPenSampleSelectedEvent(e.model.sample.prompt));
  }
}

customElements.define(SeaPenSamplesElement.is, SeaPenSamplesElement);
