// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The element for displaying an ambient theme.
 */

import 'chrome://resources/ash/common/personalization/common.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_auto_img/cr_auto_img.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';

import {assertNotReached} from 'chrome://resources/js/assert.js';

import {AmbientTheme} from '../../personalization_app.mojom-webui.js';
import {WithPersonalizationStore} from '../personalization_store.js';
import {isSelectionEvent} from '../utils.js';

import {setAmbientTheme} from './ambient_controller.js';
import {getAmbientProvider} from './ambient_interface_provider.js';
import {getTemplate} from './ambient_theme_item_element.html.js';

export class AmbientThemeItemElement extends WithPersonalizationStore {
  static get is() {
    return 'ambient-theme-item';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      ambientTheme: AmbientTheme,
      itemDescription_: {
        type: String,
        computed: 'computeItemDescription_(ambientTheme)',
      },
      imgSrc_: {
        type: String,
        computed: 'computeImgSrc_(ambientTheme)',
      },
    };
  }

  ambientTheme: AmbientTheme;
  private itemDescription_: string;
  private imgSrc_: string;

  override ready() {
    super.ready();

    this.addEventListener('click', this.onItemSelected_.bind(this));
    this.addEventListener('keydown', this.onItemSelected_.bind(this));
  }

  /** Compute the ambient theme description. */
  private computeItemDescription_(): string {
    switch (this.ambientTheme) {
      case AmbientTheme.kSlideshow:
        return this.i18n('ambientModeAnimationSlideshowLabel');
      case AmbientTheme.kFeelTheBreeze:
        return this.i18n('ambientModeAnimationFeelTheBreezeLabel');
      case AmbientTheme.kFloatOnBy:
        return this.i18n('ambientModeAnimationFloatOnByLabel');
      case AmbientTheme.kVideo:
        return this.i18n('ambientModeAnimationVideoLabel');
      default:
        assertNotReached('Invalid AmbientTheme value: ${this.ambientTheme}');
    }
  }

  /** Return the display image for ambient theme option. */
  private computeImgSrc_(ambientTheme: AmbientThemeItemElement['ambientTheme']):
      string {
    switch (ambientTheme) {
      case AmbientTheme.kSlideshow:
        return 'chrome://personalization/images/slideshow.png';
      case AmbientTheme.kFeelTheBreeze:
        return 'chrome://personalization/images/feel_the_breeze.png';
      case AmbientTheme.kFloatOnBy:
        return 'chrome://personalization/images/float_on_by.png';
      case AmbientTheme.kVideo:
        return 'chrome://personalization/time_of_day/thumbnails/new_mexico.jpg';
      default:
        assertNotReached('invalid ambient theme value.');
    }
  }

  /** Invoked when item is selected. */
  private onItemSelected_(event: Event) {
    if (!isSelectionEvent(event)) {
      return;
    }

    event.preventDefault();
    event.stopPropagation();
    setAmbientTheme(this.ambientTheme, getAmbientProvider(), this.getStore());
  }
}

customElements.define(AmbientThemeItemElement.is, AmbientThemeItemElement);
