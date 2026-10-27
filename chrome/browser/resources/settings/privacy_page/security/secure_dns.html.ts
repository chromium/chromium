// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {SecureDnsResolverType} from './secure_dns.js';
import type {SettingsSecureDnsElement} from './secure_dns.js';

export function getHtml(this: SettingsSecureDnsElement) {
  return html`<!--_html_template_start_-->
    <settings-toggle-button
        id="secureDnsToggle"
        class="hr"
        pref-key="synthetic.settings.dns_over_https.secure_dns_toggle"
        label="$i18n{secureDns}"
        sub-label="${this.secureDnsDescription_}"
        @change="${this.onToggleChange_}">
    </settings-toggle-button>

    <cr-collapse id="resolverOptions" ?opened="${this.showSecureDnsOptions_}">
      <div class="cr-row continuation">
        <div id="resolverLabel" class="flex cr-padded-text">
          $i18n{secureDnsSecureDropdownModeDescription}
        </div>

        <select id="resolverSelect"
            class="md-select"
            aria-labelledby="resolverLabel"
            @change="${this.onDropdownSelectionChange_}">
          <option value="${SecureDnsResolverType.AUTOMATIC}">
            $i18n{secureDnsAutomaticModeDescription}
          </option>
          <option value="${SecureDnsResolverType.CUSTOM}">
            $i18n{secureDnsCustomProviderDescription}
          </option>
          ${this.resolverOptions_.map((item, index) => html`
            <option data-resolver-type="${SecureDnsResolverType.BUILT_IN}"
                value="${index}">
              ${item.name}
            </option>
          `)}
        </select>
      </div>

      <div id="privacyPolicy">
        <div class="cr-secondary-text"
            .innerHTML="${this.privacyPolicyString_}"></div>
      </div>

      <div id="secureDnsInputContainer" class="cr-row continuation">
        <secure-dns-input id="secureDnsInput"
            .value="${this.secureDnsInputValue_}"
            @value-update="${this.onSecureDnsInputValueUpdate_}">
        </secure-dns-input>
      </div>
    </cr-collapse>
<!--_html_template_end_-->`;
}
