// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'certificate-entry' component is for showing a summary
 * of a certificate in a row on screen.
 *
 * This component is used in the new Certificate Management UI in
 * ./certificate_manager.ts.
 */

import '/strings.m.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_icons.css.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/cr_elements/cr_shared_style.css.js';
import '//resources/cr_elements/cr_shared_vars.css.js';

import type {CrInputElement} from '//resources/cr_elements/cr_input/cr_input.js';
import {I18nMixin} from '//resources/cr_elements/i18n_mixin.js';
import {assert} from '//resources/js/assert.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './certificate_entry.html.js';
import type {ActionResult, CertificateSource} from './certificate_manager.mojom-webui.js';
import {CertificatesBrowserProxy} from './certificates_browser_proxy.js';

export interface CertificateEntryElement {
  $: {
    certhash: CrInputElement,
    copy: HTMLElement,
    view: HTMLElement,
    delete: HTMLElement,
  };
}

declare global {
  interface HTMLElementEventMap {
    'hash-copied': CustomEvent<void>;
    'delete-result': CustomEvent<ActionResult|null>;
  }
}

const CertificateEntryElementBase = I18nMixin(PolymerElement);

export class CertificateEntryElement extends CertificateEntryElementBase {
  static get is() {
    return 'certificate-entry';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      certSource: Number,
      sha256hashHex: String,
      displayName: String,
      isDeletable: Boolean,
      showEditIcon: {
        type: Boolean,
        value: false,
      },
    };
  }

  declare certSource: CertificateSource;
  declare sha256hashHex: string;
  declare displayName: string;
  declare isDeletable: boolean;
  declare showEditIcon: boolean;

  private certDetailsIconClass_(): string {
    if (this.showEditIcon) {
      return 'icon-edit';
    } else {
      return 'icon-visibility';
    }
  }

  private onViewCertificate_() {
    CertificatesBrowserProxy.getInstance().handler.viewCertificate(
        this.certSource, this.sha256hashHex);
  }

  private onDeleteCertificate_() {
    assert(this.isDeletable);
    CertificatesBrowserProxy.getInstance()
        .handler
        .deleteCertificate(
            this.certSource, this.displayName, this.sha256hashHex)
        .then((value: {result: ActionResult|null}) => {
          this.dispatchEvent(new CustomEvent('delete-result', {
            composed: true,
            bubbles: true,
            detail: value.result,
          }));
        });
  }

  private onCopyHash_() {
    navigator.clipboard.writeText(this.sha256hashHex);
    this.dispatchEvent(
        new CustomEvent('hash-copied', {composed: true, bubbles: true}));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'certificate-entry': CertificateEntryElement;
  }
}

customElements.define(CertificateEntryElement.is, CertificateEntryElement);
