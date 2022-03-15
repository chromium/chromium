// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The element for displaying a list of animation themes.
 */
import './animation_theme_item_element.js';

import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AnimationTheme} from '../personalization_app.mojom-webui.js';
import {WithPersonalizationStore} from '../personalization_store.js';

export class AnimationThemeList extends WithPersonalizationStore {
  static get is() {
    return 'animation-theme-list';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      animationThemes: {
        type: Array,
        value: [
          AnimationTheme.kSlideshow, AnimationTheme.kFeelTheBreeze,
          AnimationTheme.kFloatOnBy
        ],
      },

      selectedAnimationTheme: AnimationTheme,

      disabled: Boolean,
    };
  }

  animationThemes: Array<AnimationTheme>;
  disabled: boolean;
  private selectedAnimationTheme: AnimationTheme;

  private isSelected_(
      animationTheme: AnimationTheme, selectedAnimationTheme: AnimationTheme) {
    return animationTheme === selectedAnimationTheme;
  }
}

customElements.define(AnimationThemeList.is, AnimationThemeList);
