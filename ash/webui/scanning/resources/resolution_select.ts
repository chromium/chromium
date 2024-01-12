// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './scan_settings_section.js';
import './strings.m.js';

import {assert} from 'chrome://resources/ash/common/assert.js';
import {I18nMixin, I18nMixinInterface} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './resolution_select.html.js';
import {AbstractConstructor, SelectMixin, SelectMixinInterface} from './select_mixin.js';

const DEFAULT_RESOLUTION: number = 300;

/**
 * @fileoverview
 * 'resolution-select' displays the available scan resolutions in a dropdown.
 */

const ResolutionSelectElementBase = SelectMixin(I18nMixin(PolymerElement)) as
        AbstractConstructor<SelectMixinInterface<number>>&
    {new (): PolymerElement & I18nMixinInterface};

export class ResolutionSelectElement extends ResolutionSelectElementBase {
  static get is() {
    return 'resolution-select' as const;
  }

  static get template() {
    return getTemplate();
  }

  override getOptionAtIndex(index: number): string {
    assert(index < this.options.length);

    return this.options[index].toString();
  }

  getResolutionString(resolution: number): string {
    return this.i18n('resolutionOptionText', resolution);
  }

  override sortOptions(): void {
    // Sort the resolutions in descending order.
    this.options.sort(function(a: number, b: number): number {
      return b - a;
    });
  }

  override isDefaultOption(option: number): boolean {
    return option === DEFAULT_RESOLUTION;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [ResolutionSelectElement.is]: ResolutionSelectElement;
  }
}

customElements.define(ResolutionSelectElement.is, ResolutionSelectElement);
