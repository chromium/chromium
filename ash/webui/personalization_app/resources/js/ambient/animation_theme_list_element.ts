// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The element for displaying a list of animation themes.
 */
import './animation_theme_item_element.js';
import '../../css/common.css.js';

import {AnimationTheme} from '../../personalization_app.mojom-webui.js';
import {WithPersonalizationStore} from '../personalization_store.js';

import {getTemplate} from './animation_theme_list_element.html.js';

export class AnimationThemeList extends WithPersonalizationStore {
  static get is() {
    return 'animation-theme-list';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      animationThemes: {
        type: Array,
        value: [
          AnimationTheme.kSlideshow,
          AnimationTheme.kFeelTheBreeze,
          AnimationTheme.kFloatOnBy,
        ],
      },

      selectedAnimationTheme: AnimationTheme,
    };
  }

  animationThemes: AnimationTheme[];
  private selectedAnimationTheme: AnimationTheme;

  private getAriaChecked_(
      animationTheme: AnimationTheme,
      selectedAnimationTheme: AnimationTheme): string {
    return (animationTheme === selectedAnimationTheme).toString();
  }
}

customElements.define(AnimationThemeList.is, AnimationThemeList);
