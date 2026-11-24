// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getTabGroupColorVar} from './tab_group_color_helper.js';
import {getCss} from './tab_group_dot.css.js';
import {getHtml} from './tab_group_dot.html.js';
import {Color} from './tab_group_types.mojom-webui.js';

export enum TabGroupDotSize {
  SMALL = 'small',
  LARGE = 'large',
}

export class TabGroupDotElement extends CrLitElement {
  static get is() {
    return 'tab-group-dot';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      color: {type: Number},
      filled: {
        type: Boolean,
        reflect: true,
      },
      size: {
        type: String,
        reflect: true,
      },
    };
  }

  accessor color: Color = Color.kGrey;
  accessor filled: boolean = true;
  accessor size: TabGroupDotSize = TabGroupDotSize.SMALL;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('color')) {
      this.style.setProperty(
          '--group-dot-color', getTabGroupColorVar(this.color));
    }

    if (changedProperties.has('size')) {
      assert(Object.values(TabGroupDotSize).includes(this.size));
    }
  }

  protected getViewBox_(): string {
    return this.size === TabGroupDotSize.LARGE ? '-6 -6 12 12' : '-3 -3 6 6';
  }

  private getOuterRadius_(): number {
    return this.size === TabGroupDotSize.LARGE ? 6 : 3;
  }

  protected getRadius_(): number {
    const outerRadius = this.getOuterRadius_();
    if (this.filled) {
      return outerRadius;
    }
    return outerRadius - this.getStrokeWidth_() / 2;
  }

  protected getStrokeWidth_(): number {
    if (this.filled) {
      return 0;
    }
    return 2;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tab-group-dot': TabGroupDotElement;
  }
}

customElements.define(TabGroupDotElement.is, TabGroupDotElement);
