// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './ai_site_add_dialog.css.js';
import {getHtml} from './ai_site_add_dialog.html.js';

export interface AiSiteAddDialogElement {
  $: {
    add: CrButtonElement,
    dialog: CrDialogElement,
    site: CrInputElement,
  };
}

export class AiSiteAddDialogElement extends CrLitElement {
  static get is() {
    return 'ai-site-add-dialog';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      site: {type: String},
      errorMessage_: {type: String},
      submitDisabled_: {type: Boolean},
    };
  }

  accessor site: string = '';
  protected accessor errorMessage_: string = '';
  protected accessor submitDisabled_: boolean = true;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('site')) {
      this.validate_();
    }
  }

  protected onSiteValueChanged_(e: CustomEvent<{value: string}>) {
    this.site = e.detail.value;
  }

  private validate_() {
    const site = this.site.trim();
    if (!site) {
      this.submitDisabled_ = true;
      this.errorMessage_ = '';
      return;
    }

    // Basic domain validation.
    // Must contain at least one dot. No consecutive dots.
    // Parts can contain hyphens but must start/end with alphanumeric
    // characters.
    const domainRegex =
        /^([a-zA-Z0-9]([a-zA-Z0-9-]*[a-zA-Z0-9])?\.)+[a-zA-Z0-9]([a-zA-Z0-9-]*[a-zA-Z0-9])?$/;
    const isValid = domainRegex.test(site);

    this.submitDisabled_ = !isValid;
    this.errorMessage_ = isValid ? '' : 'Invalid site domain.';
  }

  protected onCancelClick_() {
    this.$.dialog.cancel();
  }

  protected onSubmitClick_() {
    if (this.submitDisabled_) {
      return;
    }

    this.fire('add-site', this.site.trim().toLowerCase());
    this.$.dialog.close();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ai-site-add-dialog': AiSiteAddDialogElement;
  }
}

customElements.define(AiSiteAddDialogElement.is, AiSiteAddDialogElement);
