// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './filter_dialog_footer.css.js';
import {getHtml} from './filter_dialog_footer.html.js';

export class FilterDialogFooterElement extends CrLitElement {
  static get is() {
    return 'filter-dialog-footer';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  protected onCancelClick() {
    this.fire('cancel-click');
  }

  protected onApplyClick() {
    this.fire('apply-click');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'filter-dialog-footer': FilterDialogFooterElement;
  }
}

customElements.define(FilterDialogFooterElement.is, FilterDialogFooterElement);
