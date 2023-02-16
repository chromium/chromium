// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The element for displaying an animation theme.
 */

import '../../css/common.css.js';
import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';

import {assertNotReached} from 'chrome://resources/js/assert_ts.js';

import {AnimationTheme} from '../../personalization_app.mojom-webui.js';
import {WithPersonalizationStore} from '../personalization_store.js';
import {getCheckmarkIcon, isSelectionEvent} from '../utils.js';

import {setAnimationTheme} from './ambient_controller.js';
import {getAmbientProvider} from './ambient_interface_provider.js';
import {getTemplate} from './animation_theme_item_element.html.js';

export class AnimationThemeItem extends WithPersonalizationStore {
  static get is() {
    return 'animation-theme-item';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      animationTheme: AnimationTheme,
      itemDescription_: {
        type: String,
        computed: 'computeItemDescription_(animationTheme)',
      },
      imgSrc_: {
        type: String,
        computed: 'computeImgSrc_(animationTheme)',
      },
      checkmarkIcon_: {
        type: String,
        value() {
          return getCheckmarkIcon();
        },
      },
    };
  }

  animationTheme: AnimationTheme;
  private itemDescription_: string;
  private imgSrc_: string;

  override ready() {
    super.ready();

    this.addEventListener('click', this.onItemSelected_.bind(this));
    this.addEventListener('keydown', this.onItemSelected_.bind(this));
  }

  /** Compute the animation theme description. */
  private computeItemDescription_(): string {
    switch (this.animationTheme) {
      case AnimationTheme.kSlideshow:
        return this.i18n('ambientModeAnimationSlideshowLabel');
      case AnimationTheme.kFeelTheBreeze:
        return this.i18n('ambientModeAnimationFeelTheBreezeLabel');
      case AnimationTheme.kFloatOnBy:
        return this.i18n('ambientModeAnimationFloatOnByLabel');
      default:
        assertNotReached(
            'Invalid AnimationTheme value: ${this.animationTheme}');
    }
  }

  /** Return the display image for animation theme option. */
  private computeImgSrc_(animationTheme: AnimationThemeItem['animationTheme']):
      string {
    switch (animationTheme) {
      case AnimationTheme.kSlideshow:
        return 'chrome://personalization/images/slideshow.png';
      case AnimationTheme.kFeelTheBreeze:
        return 'chrome://personalization/images/feel_the_breeze.png';
      case AnimationTheme.kFloatOnBy:
        return 'chrome://personalization/images/float_on_by.png';
      default:
        assertNotReached('invalid animation theme value.');
    }
  }

  /** Invoked when item is selected. */
  private onItemSelected_(event: Event) {
    if (!isSelectionEvent(event)) {
      return;
    }

    event.preventDefault();
    event.stopPropagation();
    setAnimationTheme(
        this.animationTheme, getAmbientProvider(), this.getStore());
  }
}

customElements.define(AnimationThemeItem.is, AnimationThemeItem);
