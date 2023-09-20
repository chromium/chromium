// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './scan_settings_section.js';
import './strings.m.js';

import {assert} from 'chrome://resources/ash/common/assert.js';
import {I18nBehavior} from 'chrome://resources/ash/common/i18n_behavior.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './resolution_select.html.js';
import {SelectBehavior, SelectBehaviorInterface} from './select_behavior.js';

const DEFAULT_RESOLUTION: number = 300;

/**
 * @fileoverview
 * 'resolution-select' displays the available scan resolutions in a dropdown.
 */

// TODO(b/300484132): Replace mixinBehavior with mixin implementation once a
// Mixin version of SelectBehavior is available.
const ResolutionSelectElementBase =
    mixinBehaviors([I18nBehavior, SelectBehavior], PolymerElement) as
    {new (): PolymerElement & I18nBehavior & SelectBehaviorInterface};

class ResolutionSelectElement extends ResolutionSelectElementBase {
  static get is() {
    return 'resolution-select' as const;
  }

  static get template() {
    return getTemplate();
  }

  getOptionAtIndex(index: number): string {
    assert(index < this.options.length);

    return this.options[index].toString();
  }

  getResolutionString(resolution: number): string {
    return this.i18n('resolutionOptionText', resolution);
  }

  sortOptions(): void {
    // Sort the resolutions in descending order.
    this.options.sort(function(a: number, b: number): number {
      return b - a;
    });
  }

  isDefaultOption(option: number): boolean {
    return option === DEFAULT_RESOLUTION;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [ResolutionSelectElement.is]: ResolutionSelectElement;
  }
}

customElements.define(ResolutionSelectElement.is, ResolutionSelectElement);
