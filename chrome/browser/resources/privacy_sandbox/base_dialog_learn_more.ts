// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_collapse/cr_collapse.js';
import '//resources/cr_elements/cr_expand_button/cr_expand_button.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {CrCollapseElement} from 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';

import {getCss} from './base_dialog_learn_more.css.js';
import {getHtml} from './base_dialog_learn_more.html.js';

export interface BaseDialogLearnMore {
  $: {
    collapse: CrCollapseElement,
  };
}

export class BaseDialogLearnMore extends CrLitElement {
  static get is() {
    return 'base-dialog-learn-more';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      expanded_: {type: Boolean, notify: true},
      title: {type: String},
    };
  }

  accessor expanded_: boolean = false;

  protected onExpandedChanged_(e: CustomEvent<{value: boolean}>) {
    this.expanded_ = e.detail.value;
    if (this.expanded_) {
      requestAnimationFrame(() => {
        this.$.collapse.scrollIntoView({block: 'start', behavior: 'smooth'});
      });
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'base-dialog-learn-more': BaseDialogLearnMore;
  }
}

customElements.define(BaseDialogLearnMore.is, BaseDialogLearnMore);
