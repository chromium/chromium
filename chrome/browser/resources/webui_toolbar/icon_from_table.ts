// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon/cr_icon.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './icon_from_table.css.js';
import {getHtml} from './icon_from_table.html.js';
import {IconTable} from './icon_table.js';
import type {IconInfo} from './icon_table.js';
import type {IconHandle} from './toolbar_ui_api_data_model.mojom-webui.js';

// Size is controlled by --icon-size CSS variable.
export class IconFromTableElement extends CrLitElement {
  static get is() {
    return 'icon-from-table';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /**
       * Handle to the icon to show here. See toolbar_ui_api.mojom.IconHandle.
       */
      iconHandle: {type: Object},

      /**
       * Internally cached information on the icon.
       */
      iconInfo_: {type: Object},
    };
  }

  accessor iconHandle: IconHandle = {
    handleId: 0n,
  };

  protected accessor iconInfo_: IconInfo;

  private iconTable_: IconTable = IconTable.getInstance();

  constructor() {
    super();
    this.iconInfo_ = this.iconTable_.getIconInfo(this.iconHandle);
  }

  override willUpdate(changedProperties: PropertyValues<this>): void {
    super.willUpdate(changedProperties);
    if (changedProperties.has('iconHandle')) {
      this.iconInfo_ = this.iconTable_.getIconInfo(this.iconHandle);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'icon-from-table': IconFromTableElement;
  }
}

customElements.define(IconFromTableElement.is, IconFromTableElement);
