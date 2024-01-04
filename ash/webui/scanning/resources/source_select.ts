// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './scan_settings_section.js';
import './strings.m.js';

import {assert} from 'chrome://resources/ash/common/assert.js';
import {I18nMixin, I18nMixinInterface} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ScanSource, SourceType} from './scanning.mojom-webui.js';
import {alphabeticalCompare, getSourceTypeString} from './scanning_app_util.js';
import {AbstractConstructor, SelectMixin, SelectMixinInterface} from './select_mixin.js';
import {getTemplate} from './source_select.html.js';

const DEFAULT_SOURCE_TYPE: SourceType = SourceType.kFlatbed;

/**
 * @fileoverview
 * 'source-select' displays the available scanner sources in a dropdown.
 */

const SourceSelectElementBase = SelectMixin(I18nMixin(PolymerElement)) as
        AbstractConstructor<SelectMixinInterface<ScanSource>>&
    {new (): PolymerElement & I18nMixinInterface};

export class SourceSelectElement extends SourceSelectElementBase {
  static get is() {
    return 'source-select' as const;
  }

  static get template() {
    return getTemplate();
  }

  override getOptionAtIndex(index: number): string {
    assert(index < this.options.length);

    return this.options[index].name;
  }

  private getSourceTypeAsString(mojoSourceType: SourceType): string {
    return getSourceTypeString(mojoSourceType);
  }

  override sortOptions(): void {
    this.options.sort((a: ScanSource, b: ScanSource) => {
      return alphabeticalCompare(
          getSourceTypeString(a.type), getSourceTypeString(b.type));
    });
  }

  override isDefaultOption(option: ScanSource): boolean {
    return option.type === DEFAULT_SOURCE_TYPE;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SourceSelectElement.is]: SourceSelectElement;
  }
}

customElements.define(SourceSelectElement.is, SourceSelectElement);
