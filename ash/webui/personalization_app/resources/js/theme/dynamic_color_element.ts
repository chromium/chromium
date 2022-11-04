// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This component displays the dynamic color options.
 */

import '../../css/common.css.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';

import {WithPersonalizationStore} from '../personalization_store.js';

import {getTemplate} from './dynamic_color_element.html.js';

export interface DynamicColorScheme {
  id: string;
  primaryColor: string;
  secondaryColor: string;
  tertiaryColor: string;
}

export class DynamicColorElement extends WithPersonalizationStore {
  static get is() {
    return 'dynamic-color';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      // Whether or not to use the wallpaper to calculate the seed color.
      automaticSeedColorEnabled: {
        type: Boolean,
        value: true,
        notify: true,
        reflectToAttribute: true,
      },
      staticColors_: {
        type: Object,
        readOnly: true,
        value: [
          // TODO(b/254479499): Replace colors when the spec is ready.
          'var(--google-blue-500)',
          'var(--google-grey-400)',
          '#EDD0E4',
          '#EADECD',
        ],
      },
      schemes_: {
        type: Object,
        readOnly: true,
        value(): DynamicColorScheme[] {
          return [
            // TODO(254479725): Replace with colors fetched from the
            // backend.
            {
              id: 'tonal',
              primaryColor: 'var(--google-blue-500)',
              secondaryColor: 'var(--google-red-500)',
              tertiaryColor: 'var(--google-green-500)',
            },
            {
              id: 'neutral',
              primaryColor: 'var(--google-red-500)',
              secondaryColor: 'var(--google-blue-500)',
              tertiaryColor: 'var(--google-green-500)',
            },
            {
              id: 'vibrant',
              primaryColor: 'var(--google-green-500)',
              secondaryColor: 'var(--google-red-500)',
              tertiaryColor: 'var(--google-blue-500)',
            },
            {
              id: 'expressive',
              primaryColor: 'var(--google-orange-500)',
              secondaryColor: 'var(--google-red-500)',
              tertiaryColor: 'var(--google-green-500)',
            },
          ];
        },
      },
    };
  }

  automaticSeedColorEnabled: boolean;
  private staticColors_: string[];
  private schemes_: DynamicColorScheme[];
}

customElements.define(DynamicColorElement.is, DynamicColorElement);
