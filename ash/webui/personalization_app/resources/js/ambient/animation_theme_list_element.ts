// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The element for displaying a list of animation themes.
 */
import './animation_theme_item_element.js';
import '../../css/common.css.js';

import {IronListElement} from 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';

import {AnimationTheme} from '../../personalization_app.mojom-webui.js';
import {isTimeOfDayScreenSaverEnabled} from '../load_time_booleans.js';
import {WithPersonalizationStore} from '../personalization_store.js';

import {getTemplate} from './animation_theme_list_element.html.js';

export interface AnimationThemeList {
  $: {grid: IronListElement};
}

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
        value() {
          const themes = [
            AnimationTheme.kSlideshow,
            AnimationTheme.kFeelTheBreeze,
            AnimationTheme.kFloatOnBy,
          ];
          if (isTimeOfDayScreenSaverEnabled()) {
            themes.push(AnimationTheme.kVideo);
          }
          return themes;
        },
      },

      selectedAnimationTheme: AnimationTheme,
    };
  }

  animationThemes: AnimationTheme[];
  selectedAnimationTheme: AnimationTheme;

  private getAriaChecked_(
      animationTheme: AnimationTheme,
      selectedAnimationTheme: AnimationTheme): string {
    return (animationTheme === selectedAnimationTheme).toString();
  }
}

customElements.define(AnimationThemeList.is, AnimationThemeList);
