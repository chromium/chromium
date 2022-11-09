// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This component displays the dynamic color options.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import '../../css/common.css.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import 'chrome://resources/polymer/v3_0/iron-a11y-keys/iron-a11y-keys.js';
import 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';

import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {IronA11yKeysElement} from 'chrome://resources/polymer/v3_0/iron-a11y-keys/iron-a11y-keys.js';
import {IronSelectorElement} from 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';

import {WithPersonalizationStore} from '../personalization_store.js';

import {getTemplate} from './dynamic_color_element.html.js';

export interface DynamicColorScheme {
  id: string;
  primaryColor: string;
  secondaryColor: string;
  tertiaryColor: string;
}

export interface DynamicColorElement {
  $: {
    keys: IronA11yKeysElement,
    colorSchemeSelector: IronSelectorElement,
    staticColorSelector: IronSelectorElement,
  };
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
      // The color scheme button currently highlighted by keyboard navigation.
      colorSchemeSelectedButton_: {
        type: Object,
        notify: true,
      },
      // The static color button currently highlighted by keyboard navigation.
      staticColorSelectedButton_: {
        type: Object,
        notify: true,
      },
    };
  }

  automaticSeedColorEnabled: boolean;
  private staticColors_: string[];
  private schemes_: DynamicColorScheme[];
  private colorSchemeSelectedButton_: CrButtonElement;
  private staticColorSelectedButton_: CrButtonElement;

  override ready() {
    super.ready();
    this.$.keys.target = this.automaticSeedColorEnabled ?
        this.$.colorSchemeSelector :
        this.$.staticColorSelector;
  }

  /** Handle keyboard navigation. */
  private onKeysPress_(
      e: CustomEvent<{key: string, keyboardEvent: KeyboardEvent}>) {
    const selector = this.automaticSeedColorEnabled ?
        this.$.colorSchemeSelector :
        this.$.staticColorSelector;
    const prevButton = this.automaticSeedColorEnabled ?
        this.colorSchemeSelectedButton_ :
        this.staticColorSelectedButton_;
    switch (e.detail.key) {
      case 'left':
        selector.selectPrevious();
        break;
      case 'right':
        selector.selectNext();
        break;
      default:
        return;
    }
    // Remove focus state of previous button.
    if (prevButton) {
      prevButton.removeAttribute('tabindex');
    }
    // Add focus state for new button.
    if (this.automaticSeedColorEnabled && this.colorSchemeSelectedButton_) {
      this.colorSchemeSelectedButton_.setAttribute('tabindex', '0');
      this.colorSchemeSelectedButton_.focus();
    }
    if (!this.automaticSeedColorEnabled && this.staticColorSelectedButton_) {
      this.staticColorSelectedButton_.setAttribute('tabindex', '0');
      this.staticColorSelectedButton_.focus();
    }
    e.detail.keyboardEvent.preventDefault();
  }

  private getTabIndex_(id: string): string {
    return id === 'tonal' || id === 'var(--google-blue-500)' ? '0' : '-1';
  }
}

customElements.define(DynamicColorElement.is, DynamicColorElement);
