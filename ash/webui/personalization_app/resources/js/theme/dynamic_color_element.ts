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

import {ColorScheme, SampleColorScheme} from '../../personalization_app.mojom-webui.js';
import {WithPersonalizationStore} from '../personalization_store.js';
import {convertToRgbHexStr} from '../utils.js';

import {getTemplate} from './dynamic_color_element.html.js';
import {initializeDynamicColorData, setColorSchemePref, setStaticColorPref} from './theme_controller.js';
import {getThemeProvider} from './theme_interface_provider.js';
import {ThemeObserver} from './theme_observer.js';

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
        computed: 'isAutomaticSeedColorEnabled_(colorSchemeSelected_)',
      },
      // The static color stored in the backend.
      staticColorSelected_: Object,
      // The color scheme stored in the backend.
      colorSchemeSelected_: Object,
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
      sampleColorSchemes_: {
        type: Array,
        notify: true,
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
  private previousStaticColorSelected_: SkColor|null;
  private previousColorSchemeSelected_: ColorScheme|null;
  private staticColorSelected_: SkColor|null;
  private colorSchemeSelected_: ColorScheme|null;
  private staticColors_: string[];
  private sampleColorSchemes_: SampleColorScheme[];
  private colorSchemeHighlightedButton_: CrButtonElement;
  private staticColorHighlightedButton_: CrButtonElement;

  override ready() {
    super.ready();
    this.$.staticColorKeys.target = this.$.staticColorSelector;
    this.$.colorSchemeKeys.target = this.$.colorSchemeSelector;
  }

  override connectedCallback() {
    super.connectedCallback();
    ThemeObserver.initThemeObserverIfNeeded();
    this.watch<DynamicColorElement['staticColorSelected_']>(
        'staticColorSelected_', state => state.theme.staticColorSelected);
    this.watch<DynamicColorElement['colorSchemeSelected_']>(
        'colorSchemeSelected_', state => state.theme.colorSchemeSelected);
    this.watch<DynamicColorElement['sampleColorSchemes_']>(
        'sampleColorSchemes_', state => state.theme.sampleColorSchemes);
    this.updateFromStore();
    initializeDynamicColorData(getThemeProvider(), this.getStore());
  }

  private onClickColorSchemeButton_(event: Event) {
    const eventTarget = event.currentTarget as HTMLElement;
    const colorScheme = Number(eventTarget.dataset['colorSchemeId']);
    setColorSchemePref(colorScheme, getThemeProvider(), this.getStore());
  }

  private onClickStaticColorButton_(event: Event) {
    const eventTarget = event.currentTarget as HTMLElement;
    const staticColorHexStr = String(eventTarget.dataset['staticColor']);
    const staticColor = hexColorToSkColor(staticColorHexStr);
    setStaticColorPref(staticColor, getThemeProvider(), this.getStore());
  }

  private onToggleChanged_() {
    if (this.automaticSeedColorEnabled) {
      this.previousColorSchemeSelected_ = this.colorSchemeSelected_;
      const staticColor =
          this.previousStaticColorSelected_ || DEFAULT_STATIC_COLOR;
      setStaticColorPref(staticColor, getThemeProvider(), this.getStore());
    } else {
      this.previousStaticColorSelected_ = this.staticColorSelected_;
      const colorScheme =
          this.previousColorSchemeSelected_ || DEFAULT_COLOR_SCHEME;
      setColorSchemePref(colorScheme, getThemeProvider(), this.getStore());
    }
  }

  private isAutomaticSeedColorEnabled_(colorScheme: ColorScheme|null) {
    return colorScheme === null || colorScheme !== ColorScheme.kStatic;
  }

  private getColorSchemeAriaChecked_(
      colorScheme: number, colorSchemeSelected: number|null): 'true'|'false' {
    const checkedColorScheme = colorSchemeSelected || DEFAULT_COLOR_SCHEME;
    return checkedColorScheme === colorScheme ? 'true' : 'false';
  }

  private getStaticColorAriaChecked_(
      staticColor: string, staticColorSelected: SkColor|null): 'true'|'false' {
    const checkedStaticColor = staticColorSelected || DEFAULT_STATIC_COLOR;
    return staticColor === convertToRgbHexStr(checkedStaticColor.value) ?
        'true' :
        'false';
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

  /**
   * Returns the tab index for static color and color scheme buttons. Static
   * color id is a string whereas color scheme id is an enum.
   */
  private getTabIndex_(id: string|number): string {
    if (typeof id === 'string' &&
        hexColorToSkColor(id).value === DEFAULT_STATIC_COLOR.value) {
      // Handles static color.
      return '0';
    } else if (typeof id === 'number' && id === DEFAULT_COLOR_SCHEME) {
      // Handles color scheme.
      return '0';
    }
    return '-1';
  }
}

customElements.define(DynamicColorElement.is, DynamicColorElement);
