// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {SafeBrowsingSetting} from '../security/safe_browsing_types.js';

import type {PrivacyGuideSafeBrowsingFragmentElement} from './privacy_guide_safe_browsing_fragment.js';

export function getHtml(this: PrivacyGuideSafeBrowsingFragmentElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="settings-fragment-header" focus-element tabindex="-1">
  <picture>
    <source
        srcset="./images/privacy_guide/safe_browsing_graphic_dark_v2.svg"
        media="(prefers-color-scheme: dark)">
    <img alt="" src="./images/privacy_guide/safe_browsing_graphic_v2.svg">
  </picture>
  <h2 class="settings-fragment-header-label">
    $i18n{privacyGuideSafeBrowsingCardHeader}
  </h2>
</div>
<div class="fragment-content">
  <settings-radio-group id="safeBrowsingRadioGroup"
      pref-key="generated.safe_browsing"
      selectable-elements="settings-collapse-radio-button">
    <settings-collapse-radio-button id="safeBrowsingRadioEnhanced"
        pref-key="generated.safe_browsing"
        name="${SafeBrowsingSetting.ENHANCED}"
        label="$i18n{safeBrowsingEnhanced}"
        sub-label="$i18n{safeBrowsingEnhancedDescUpdated}"
        expand-aria-label="$i18n{safeBrowsingEnhancedExpandA11yLabel}"
        @click="${this.onSafeBrowsingEnhancedClick_}">
          <div slot="collapse" class="settings-columned-section">
            <div class="column">
              <h3 class="description-header">
                $i18n{columnHeadingWhenOn}
              </h3>
              <ul id="updatedDescItemContainer" class="icon-bulleted-list">
                <li>
                  <cr-icon icon="settings20:bar-chart" aria-hidden="true">
                  </cr-icon>
                  <div class="secondary">
                    $i18n{safeBrowsingEnhancedWhenOnBulOne}
                  </div>
                </li>
                <li>
                  <cr-icon icon="settings20:download" aria-hidden="true">
                  </cr-icon>
                  <div class="secondary">
                    $i18n{safeBrowsingEnhancedWhenOnBulTwo}
                  </div>
                </li>
                <li>
                  <cr-icon icon="settings20:gshield" aria-hidden="true">
                  </cr-icon>
                  <div class="secondary">
                    $i18n{safeBrowsingEnhancedWhenOnBulThree}
                  </div>
                </li>
                <li>
                  <cr-icon icon="settings:language" aria-hidden="true">
                  </cr-icon>
                  <div class="secondary">
                    $i18n{safeBrowsingEnhancedWhenOnBulFour}
                  </div>
                </li>
              </ul>
            </div>
            <div class="column">
              <h3 class="description-header">
                $i18n{columnHeadingConsider}
              </h3>
              <ul class="icon-bulleted-list">
                <li>
                  <cr-icon icon="settings20:link" aria-hidden="true">
                  </cr-icon>
                  <div class="secondary">
                    $i18n{safeBrowsingEnhancedThingsToConsiderBulOne}
                  </div>
                </li>
                <li>
                  <cr-icon icon="settings20:account-circle" aria-hidden="true">
                  </cr-icon>
                  <div class="secondary">
                    $i18n{safeBrowsingEnhancedThingsToConsiderBulTwo}
                  </div>
                </li>
                <li>
                  <cr-icon icon="settings:speed" aria-hidden="true">
                  </cr-icon>
                  <div class="secondary">
                    $i18n{safeBrowsingEnhancedThingsToConsiderBulThree}
                  </div>
                </li>
              </ul>
            </div>
          </div>
    </settings-collapse-radio-button>
    <settings-collapse-radio-button id="safeBrowsingRadioStandard"
        no-collapse
        pref-key="generated.safe_browsing"
        name="${SafeBrowsingSetting.STANDARD}"
        label="$i18n{safeBrowsingStandard}"
        sub-label="${this.getSafeBrowsingStandardSubLabel_()}"
        expand-aria-label="$i18n{safeBrowsingStandardExpandA11yLabel}"
        @click="${this.onSafeBrowsingStandardClick_}">
    </settings-collapse-radio-button>
  </settings-radio-group>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
