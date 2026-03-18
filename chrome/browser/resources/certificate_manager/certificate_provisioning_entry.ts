// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'certificate-provisioning-entry' is an element that displays
 * one certificate provisioning processes.
 */
import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';

import type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrLazyRenderLitElement} from '//resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {CertificateProvisioningViewDetailsActionEvent} from './certificate_manager_types.js';
import type {CertificateProvisioningProcess} from './certificate_provisioning_browser_proxy.js';
import {getCss} from './certificate_provisioning_entry.css.js';
import {getHtml} from './certificate_provisioning_entry.html.js';

export interface CertificateProvisioningEntryElement {
  $: {
    dots: HTMLElement,
    menu: CrLazyRenderLitElement<CrActionMenuElement>,
  };
}

export class CertificateProvisioningEntryElement extends CrLitElement {
  static get is() {
    return 'certificate-provisioning-entry';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      model: {type: Object},
    };
  }

  accessor model: CertificateProvisioningProcess = {
    processId: '',
    certProfileId: '',
    certProfileName: '',
    isDeviceWide: false,
    lastUnsuccessfulMessage: '',
    status: '',
    stateId: 0,
    timeSinceLastUpdate: '',
    publicKey: '',
  };

  private closePopupMenu_() {
    this.$.menu.get().close();
  }

  protected onDotsClick_() {
    this.$.menu.get().showAt(this.$.dots);
  }

  protected onDetailsClick_() {
    this.closePopupMenu_();
    this.fire(CertificateProvisioningViewDetailsActionEvent, {
      model: this.model,
      anchor: this.$.dots,
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'certificate-provisioning-entry': CertificateProvisioningEntryElement;
  }
}

customElements.define(
    CertificateProvisioningEntryElement.is,
    CertificateProvisioningEntryElement);
