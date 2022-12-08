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
import {hexColorToSkColor} from 'chrome://resources/js/color_utils.js';
import {SkColor} from 'chrome://resources/mojo/skia/public/mojom/skcolor.mojom-webui.js';
import {IronA11yKeysElement} from 'chrome://resources/polymer/v3_0/iron-a11y-keys/iron-a11y-keys.js';
import {IronSelectorElement} from 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';

import {ColorScheme} from '../personalization_app.mojom-webui.js';
import {WithPersonalizationStore} from '../personalization_store.js';

import {getTemplate} from './dynamic_color_element.html.js';
import {setColorSchemePref, setStaticColorPref} from './theme_controller.js';
import {getThemeProvider} from './theme_interface_provider.js';

export interface DynamicColorScheme {
  id: ColorScheme;
  primaryColor: string;
  secondaryColor: string;
  tertiaryColor: string;
}

export interface DynamicColorElement {
  $: {
    staticColorKeys: IronA11yKeysElement,
    colorSchemeKeys: IronA11yKeysElement,
    colorSchemeSelector: IronSelectorElement,
    staticColorSelector: IronSelectorElement,
  };
}

const DEFAULT_STATIC_COLOR = hexColorToSkColor('#4285f4');
const DEFAULT_COLOR_SCHEME = ColorScheme.kTonalSpot;

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
      // The static color stored in the backend.
      staticColorSelected_: {
        type: Object,
        value: DEFAULT_STATIC_COLOR,
      },
      // The color scheme stored in the backend.
      colorSchemeSelected_: {
        type: Object,
        value: DEFAULT_COLOR_SCHEME,
      },
      staticColors_: {
        type: Object,
        readOnly: true,
        value: [
          // TODO(b/254479499): Replace colors when the spec is ready.
          '#4285f4',
          '#bdc1c6',
          '#edd0e4',
          '#eadecd',
        ],
      },
      colorSchemes_: {
        type: Object,
        readOnly: true,
        value(): DynamicColorScheme[] {
          return [
            // TODO(254479725): Replace with colors fetched from the
            // backend.
            {
              id: ColorScheme.kTonalSpot,
              primaryColor: 'var(--google-blue-500)',
              secondaryColor: 'var(--google-red-500)',
              tertiaryColor: 'var(--google-green-500)',
            },
            {
              id: ColorScheme.kNeutral,
              primaryColor: 'var(--google-red-500)',
              secondaryColor: 'var(--google-blue-500)',
              tertiaryColor: 'var(--google-green-500)',
            },
            {
              id: ColorScheme.kVibrant,
              primaryColor: 'var(--google-green-500)',
              secondaryColor: 'var(--google-red-500)',
              tertiaryColor: 'var(--google-blue-500)',
            },
            {
              id: ColorScheme.kExpressive,
              primaryColor: 'var(--google-orange-500)',
              secondaryColor: 'var(--google-red-500)',
              tertiaryColor: 'var(--google-green-500)',
            },
          ];
        },
      },
      // The color scheme button currently highlighted by keyboard navigation.
      colorSchemeHighlightedButton_: {
        type: Object,
        notify: true,
      },
      // The static color button currently highlighted by keyboard navigation.
      staticColorHighlightedButton_: {
        type: Object,
        notify: true,
      },
    };
  }

  automaticSeedColorEnabled: boolean;
  private staticColorSelected_: SkColor|null;
  private colorSchemeSelected_: ColorScheme|null;
  private staticColors_: string[];
  private colorSchemes_: DynamicColorScheme[];
  private colorSchemeHighlightedButton_: CrButtonElement;
  private staticColorHighlightedButton_: CrButtonElement;

  override ready() {
    super.ready();
    this.$.staticColorKeys.target = this.$.staticColorSelector;
    this.$.colorSchemeKeys.target = this.$.colorSchemeSelector;
  }

  private onClickColorSchemeButton_(event: Event) {
    const eventTarget = event.currentTarget as HTMLElement;
    const colorScheme = Number(eventTarget.dataset['colorSchemeId']);
    this.colorSchemeSelected_ = colorScheme;
    setColorSchemePref(colorScheme, getThemeProvider(), this.getStore());
  }

  private onClickStaticColorButton_(event: Event) {
    const eventTarget = event.currentTarget as HTMLElement;
    const staticColorHexStr = String(eventTarget.dataset['staticColor']);
    const staticColor = hexColorToSkColor(staticColorHexStr);
    this.staticColorSelected_ = staticColor;
    setStaticColorPref(staticColor, getThemeProvider(), this.getStore());
  }

  private onClickToggle_() {
    if (this.automaticSeedColorEnabled) {
      const staticColor = this.staticColorSelected_ || DEFAULT_STATIC_COLOR;
      setStaticColorPref(staticColor, getThemeProvider(), this.getStore());
    } else {
      const colorScheme = this.colorSchemeSelected_ || DEFAULT_COLOR_SCHEME;
      setColorSchemePref(colorScheme, getThemeProvider(), this.getStore());
    }
  }

  private onStaticColorKeysPress_(
      e: CustomEvent<{key: string, keyboardEvent: KeyboardEvent}>) {
    this.onKeysPress_(
        e, this.$.staticColorSelector, this.staticColorHighlightedButton_);
  }

  private onColorSchemeKeysPress_(
      e: CustomEvent<{key: string, keyboardEvent: KeyboardEvent}>) {
    this.onKeysPress_(
        e, this.$.colorSchemeSelector, this.colorSchemeHighlightedButton_);
  }

  /** Handle keyboard navigation. */
  private onKeysPress_(
      e: CustomEvent<{key: string, keyboardEvent: KeyboardEvent}>,
      selector: IronSelectorElement, prevButton: CrButtonElement) {
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
    const highlightedButton = this.automaticSeedColorEnabled ?
        this.colorSchemeHighlightedButton_ :
        this.staticColorHighlightedButton_;
    highlightedButton.setAttribute('tabindex', '0');
    highlightedButton.focus();

    e.detail.keyboardEvent.preventDefault();
  }

  private getTabIndex_(id: string): string {
    return id === String(DEFAULT_COLOR_SCHEME) ||
            hexColorToSkColor(id) === DEFAULT_STATIC_COLOR ?
        '0' :
        '-1';
  }
}

customElements.define(DynamicColorElement.is, DynamicColorElement);
