// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The element for displaying a list of ambient themes.
 */

import 'chrome://resources/ash/common/personalization/common.css.js';
import './ambient_theme_item_element.js';

import {IronListElement} from 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';

import {AmbientTheme} from '../../personalization_app.mojom-webui.js';
import {isTimeOfDayScreenSaverEnabled} from '../load_time_booleans.js';
import {WithPersonalizationStore} from '../personalization_store.js';

import {getTemplate} from './ambient_theme_list_element.html.js';

export interface AmbientThemeListElement {
  $: {grid: IronListElement};
}

export class AmbientThemeListElement extends WithPersonalizationStore {
  static get is() {
    return 'ambient-theme-list';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      ambientThemes: {
        type: Array,
        value() {
          const themes = [
            AmbientTheme.kSlideshow,
            AmbientTheme.kFeelTheBreeze,
            AmbientTheme.kFloatOnBy,
          ];
          if (isTimeOfDayScreenSaverEnabled()) {
            themes.push(AmbientTheme.kVideo);
          }
          return themes;
        },
      },

      selectedAmbientTheme: AmbientTheme,
    };
  }

  ambientThemes: AmbientTheme[];
  selectedAmbientTheme: AmbientTheme;

  private getAriaChecked_(
      ambientTheme: AmbientTheme, selectedAmbientTheme: AmbientTheme): string {
    return (ambientTheme === selectedAmbientTheme).toString();
  }
}

customElements.define(AmbientThemeListElement.is, AmbientThemeListElement);
