// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon/cr_icon.js';

import {skColorToRgba} from '//resources/js/color_utils.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './icon_from_table.css.js';
import {getHtml} from './icon_from_table.html.js';
import type {IconHandle} from './icon_handle.mojom-webui.js';
import {IconTable} from './icon_table.js';
import type {IconInfo} from './icon_table.js';

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

  // Computes the CSS needed to configure the embedded rendering element,
  // whether cr-icon or our own use of mask-image, to render the icon in
  // the color it specifies.
  //
  // Will be undefined if the icon doesn't request to be rendered in a specific
  // color (including when it's multicolor).
  protected getIconColorCss_(): string|undefined {
    if (!this.iconInfo_ || !this.iconInfo_.color) {
      return undefined;
    }
    const rgba = skColorToRgba(this.iconInfo_.color);
    return `color: ${rgba}; --iron-icon-fill-color: ${rgba};`;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'icon-from-table': IconFromTableElement;
  }
}

customElements.define(IconFromTableElement.is, IconFromTableElement);
