// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';

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

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
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

  protected onAutopushInputChange(e: Event) {
    this.data_!.config.autopushGuestUrl = (e.target as HTMLInputElement).value;
  }

  protected onStagingInputChange(e: Event) {
    this.data_!.config.stagingGuestUrl = (e.target as HTMLInputElement).value;
  }

  protected onPreprodInputChange(e: Event) {
    this.data_!.config.preprodGuestUrl = (e.target as HTMLInputElement).value;
  }

  protected onProdInputChange(e: Event) {
    this.data_!.config.prodGuestUrl = (e.target as HTMLInputElement).value;
  }

  protected onSavePresetsClick_() {
    const errorMsg =
        this.shadowRoot.querySelector<HTMLDivElement>('#inputErrorMsg');

    try {
      // Validate the URL. If we don't validate here, IPC will kill this
      // renderer on invalid URLs.
      new URL(this.data_!.config.autopushGuestUrl);
      new URL(this.data_!.config.stagingGuestUrl);
      new URL(this.data_!.config.preprodGuestUrl);
      new URL(this.data_!.config.prodGuestUrl);
    } catch {
      console.error('Invalid URL: no-op');
      errorMsg!.classList.remove('hiddenElement');
      return;
    }
    errorMsg!.classList.add('hiddenElement');
    this.browserProxy_.pageHandler.setGuestUrlPresets(
        this.data_!.config.autopushGuestUrl, this.data_!.config.stagingGuestUrl,
        this.data_!.config.preprodGuestUrl, this.data_!.config.prodGuestUrl);
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

  protected getTableData_(): Array<{label: string, value: boolean}> {
    if (!this.data_ || !this.data_.enablement) {
      return [];
    }

    return [
      {
        label: 'Enabled by Chrome Flags',
        value: !this.data_.enablement.featureDisabled,
      },
      {
        label: 'Regular profile',
        value: !this.data_.enablement.notRegularProfile,
      },
      {
        label: 'Pref or flag based rollout (flag or pref) applies',
        value: !this.data_.enablement.notRolledOut,
      },
      {
        label: 'Account exists and has the Gemini in Chrome capability',
        value: !this.data_.enablement.primaryAccountNotCapable,
      },
      {
        label: 'Account exists and is fully signed-in',
        value: !this.data_.enablement.primaryAccountNotFullySignedIn,
      },
      {
        label:
            'Chrome Enterprise policy allows this feature (or doesn\'t apply)',
        value: !this.data_.enablement.disallowedByChromePolicy,
      },
      {
        label: 'Server side admin allows this feature',
        value: !this.data_.enablement.disallowedByRemoteAdmin,
      },
      {
        label: 'Server side allows this feature (Not admin policy)',
        value: !this.data_.enablement.disallowedByRemoteOther,
      },
      {
        label: 'User did pass the FRE',
        value: !this.data_.enablement.notConsented,
      },
      {
        label: 'User accepted actuation consent',
        value: !this.data_.enablement.actuationNotConsented,
      },
    ];
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'glic-internals-app': GlicInternalsAppElement;
  }
}

customElements.define(GlicInternalsAppElement.is, GlicInternalsAppElement);
