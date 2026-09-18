// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {PrivacyGuideMsbbFragmentElement} from './privacy_guide_msbb_fragment.js';

export function getHtml(this: PrivacyGuideMsbbFragmentElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="settings-fragment-header" focus-element tabindex="-1">
  <picture>
    <source
        srcset="./images/privacy_guide/msbb_graphic_dark_v2.svg"
        media="(prefers-color-scheme: dark)">
    <img alt="" src="./images/privacy_guide/msbb_graphic_v2.svg">
  </picture>
  <h2 class="settings-fragment-header-label">
    $i18n{privacyGuideMsbbCardHeader}
  </h2>
</div>
<div class="fragment-content">
  <div class="embedded-setting-wrapper">
    <settings-toggle-button id="urlCollectionToggle"
        pref-key="url_keyed_anonymized_data_collection.enabled"
        label="$i18n{urlKeyedAnonymizedDataCollection}"
        @change="${this.onMsbbToggleChange_}">
    </settings-toggle-button>
  </div>
  <div class="settings-columned-section">
    <div class="column">
      <h3 class="description-header">
        $i18n{columnHeadingWhenOn}
      </h3>
      <ul class="icon-bulleted-list">
        <li>
          <cr-icon icon="settings20:flash-on-filled"
              aria-hidden="true"></cr-icon>
          <div class="secondary">
            $i18n{privacyGuideMsbbFeatureDescription1}
          </div>
        </li>
        <li>
          <cr-icon icon="settings20:lightbulb-2" aria-hidden="true"></cr-icon>
          <div class="secondary">
            $i18n{privacyGuideMsbbFeatureDescription2}
          </div>
        </li>
        <li>
          <cr-icon icon="settings20:notification-add" aria-hidden="true">
          </cr-icon>
          <div class="secondary">
            $i18n{privacyGuideMsbbFeatureDescription3}
          </div>
        </li>
      </ul>
    </div>
    <div class="column">
      <h3 class="description-header">$i18n{columnHeadingConsider}</h3>
      <ul class="icon-bulleted-list">
        <li>
          <cr-icon icon="settings20:link" aria-hidden="true"></cr-icon>
          <div class="secondary">
            $i18n{privacyGuideMsbbPrivacyDescription1}
          </div>
        </li>
        <li>
          <cr-icon icon="settings20:data-connectors-system" aria-hidden="true">
          </cr-icon>
          <div class="secondary">
            $i18n{privacyGuideMsbbPrivacyDescription2}
          </div>
        </li>
      </ul>
    </div>
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
