// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import '../settings_shared.css.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './ai_site_add_dialog.html.js';

export interface AiSiteAddDialogElement {
  $: {
    add: CrButtonElement,
    dialog: CrDialogElement,
    site: CrInputElement,
  };
}

export class AiSiteAddDialogElement extends PolymerElement {
  static get is() {
    return 'ai-site-add-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      site: {
        type: String,
        value: '',
      },
      site_: {
        type: String,
        value: '',
      },
      errorMessage_: {
        type: String,
        value: '',
      },
      submitDisabled_: {
        type: Boolean,
        value: true,
      },
    };
  }

  static get observers() {
    return ['validate_(site_)'];
  }

  declare site: string;
  declare private site_: string;
  declare private errorMessage_: string;
  declare private submitDisabled_: boolean;

  override connectedCallback() {
    super.connectedCallback();
    this.site_ = this.site;
    this.$.dialog.showModal();
    requestAnimationFrame(() => {
      this.$.site.focusInput();
    });
  }

  private validate_() {
    const site = this.site_.trim();
    if (!site) {
      this.$.site.invalid = false;
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

    this.$.site.invalid = !isValid;
    this.submitDisabled_ = !isValid;
    this.errorMessage_ = isValid ? '' : 'Invalid site domain.';
  }

  private onCancelClick_() {
    this.$.dialog.cancel();
  }

  private onSubmitClick_() {
    if (this.submitDisabled_) {
      return;
    }

    this.dispatchEvent(new CustomEvent('add-site', {
      bubbles: true,
      composed: true,
      detail: this.site_.trim().toLowerCase(),
    }));

    this.$.dialog.close();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ai-site-add-dialog': AiSiteAddDialogElement;
  }
}

customElements.define(AiSiteAddDialogElement.is, AiSiteAddDialogElement);
