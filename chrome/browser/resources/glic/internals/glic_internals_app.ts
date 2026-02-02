// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {BrowserProxyImpl} from '../browser_proxy.js';
import {ActuationEligibility} from '../glic.mojom-webui.js';
import type {InternalsDataPayload} from '../glic.mojom-webui.js';

import {getCss} from './glic_internals_app.css.js';
import {getHtml} from './glic_internals_app.html.js';


export class GlicInternalsAppElement extends CrLitElement {
  static get is() {
    return 'glic-internals-app';
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get styles() {
    return getCss();
  }

  static override get properties() {
    return {
      data_: {type: Object},
    };
  }

  protected accessor data_: InternalsDataPayload|undefined;

  private browserProxy_ = new BrowserProxyImpl();

  override connectedCallback() {
    super.connectedCallback();
    this.browserProxy_.pageHandler.getInternalsDataPayload().then(
        ({internalsData}) => {
          this.data_ = internalsData;
        });
  }

  protected getActuationEligibilityString_(eligibility: ActuationEligibility):
      string {
    switch (eligibility) {
      case ActuationEligibility.kEligible:
        return 'Eligible';
      case ActuationEligibility.kMissingAccountCapability:
        return 'Missing account capability';
      case ActuationEligibility.kMissingChromeBenefits:
        return 'Missing Chrome benefits';
      case ActuationEligibility.kDisabledByPolicy:
        return 'Disabled by policy';
      case ActuationEligibility.kPlatformUnsupported:
        return 'Platform unsupported';
      case ActuationEligibility.kEnterpriseWithoutManagement:
        return 'Enterprise account without management. Default pref disabled.';
      default:
        return 'unknown';
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'glic-internals-app': GlicInternalsAppElement;
  }
}

customElements.define(GlicInternalsAppElement.is, GlicInternalsAppElement);
