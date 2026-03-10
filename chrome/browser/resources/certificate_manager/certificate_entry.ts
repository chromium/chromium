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
import '//resources/cr_elements/cr_input/cr_input.js';

import type {CrInputElement} from '//resources/cr_elements/cr_input/cr_input.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './certificate_entry.css.js';
import {getHtml} from './certificate_entry.html.js';
import {CertificateSource} from './certificate_manager.mojom-webui.js';
import type {ActionResult} from './certificate_manager.mojom-webui.js';
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

const CertificateEntryElementBase = I18nMixinLit(CrLitElement);

export class CertificateEntryElement extends CertificateEntryElementBase {
  static get is() {
    return 'certificate-entry';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      certSource: {type: Number},
      sha256hashHex: {type: String},
      displayName: {type: String},
      isDeletable: {type: Boolean},
      showEditIcon: {type: Boolean},
    };
  }

  accessor certSource: CertificateSource = CertificateSource.MIN_VALUE;
  accessor sha256hashHex: string = '';
  accessor displayName: string = '';
  accessor isDeletable: boolean = false;
  accessor showEditIcon: boolean = false;

  protected certDetailsIconClass_(): string {
    if (this.showEditIcon) {
      return 'icon-edit';
    } else {
      return 'icon-visibility';
    }
  }

  protected onViewClick_() {
    CertificatesBrowserProxy.getInstance().handler.viewCertificate(
        this.certSource, this.sha256hashHex);
  }

  protected onDeleteClick_() {
    assert(this.isDeletable);
    CertificatesBrowserProxy.getInstance()
        .handler
        .deleteCertificate(
            this.certSource, this.displayName, this.sha256hashHex)
        .then((value: {result: ActionResult|null}) => {
          this.fire('delete-result', value.result);
        });
  }

  protected onCopyClick_() {
    navigator.clipboard.writeText(this.sha256hashHex);
    this.fire('hash-copied');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'certificate-entry': CertificateEntryElement;
  }
}

customElements.define(CertificateEntryElement.is, CertificateEntryElement);
