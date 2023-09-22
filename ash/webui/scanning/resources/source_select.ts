// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './scan_settings_section.js';
import './strings.m.js';

import {assert} from 'chrome://resources/ash/common/assert.js';
import {I18nBehavior} from 'chrome://resources/ash/common/i18n_behavior.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ScanSource, SourceType} from './scanning.mojom-webui.js';
import {alphabeticalCompare, getSourceTypeString} from './scanning_app_util.js';
import {SelectBehavior, SelectBehaviorInterface} from './select_behavior.js';
import {getTemplate} from './source_select.html.js';

const DEFAULT_SOURCE_TYPE: SourceType = SourceType.kFlatbed;

/**
 * @fileoverview
 * 'source-select' displays the available scanner sources in a dropdown.
 */

// TODO(b/300484132): Replace mixinBehavior with mixin implementation once a
// Mixin version of SelectBehavior is available.
const SourceSelectElementBase =
    mixinBehaviors([I18nBehavior, SelectBehavior], PolymerElement) as
    {new (): PolymerElement & I18nBehavior & SelectBehaviorInterface};

class SourceSelectElement extends SourceSelectElementBase {
  static get is() {
    return 'source-select' as const;
  }

  static get template() {
    return getTemplate();
  }

  getOptionAtIndex(index: number): string {
    assert(index < this.options.length);

    return this.options[index].name;
  }

  private getSourceTypeAsString(mojoSourceType: SourceType): string {
    return getSourceTypeString(mojoSourceType);
  }

  sortOptions(): void {
    this.options.sort((a: ScanSource, b: ScanSource) => {
      return alphabeticalCompare(
          getSourceTypeString(a.type), getSourceTypeString(b.type));
    });
  }

  isDefaultOption(option: ScanSource): boolean {
    return option.type === DEFAULT_SOURCE_TYPE;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SourceSelectElement.is]: SourceSelectElement;
  }
}

customElements.define(SourceSelectElement.is, SourceSelectElement);
