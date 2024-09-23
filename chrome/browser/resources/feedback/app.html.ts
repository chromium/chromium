// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {AppElement} from './app.js';

export function getHtml(this: AppElement) {
  return html`<!--_html_template_start_-->
<div id="content-pane" class="content">
  <p id="free-form-text">$i18n{freeFormText}</p>
  <textarea id="description-text" aria-labelledby="free-form-text"
      aria-required="true">
  </textarea>
  <div id="questionnaire-notification" aria-live="polite"
      class="off-screen">
  </div>
  <p id="description-empty-error" class="description-empty-notification"
      aria-hidden="true" hidden>$i18n{noDescription}</p>
  <div>
    <p id="additional-info-label">$i18n{additionalInfo}<p>
  </div>
  <div id="page-url" class="text-field-container">
    <label id="page-url-label">$i18n{pageUrl}</label>
    <input id="page-url-text" aria-labelledby="page-url-label" type="text"
            dir="ltr">
  </div>
  <!-- User e-mail -->
  <div id="user-email" class="text-field-container" hidden>
    <label id="user-email-label">$i18n{userEmail}</label>
    <select id="user-email-drop-down" aria-labelledby="user-email-label">
      <option id="anonymous-user-option"
              value="">$i18n{anonymousUser}</option>
    </select>
  </div>
  <!-- Attach a file -->
  <div id="attach-file-container" class="text-field-container">
    <label id="attach-file-label">$i18n{attachFileLabel}</label>
    <input id="attach-file" type="file" aria-describedby="attach-file-note">
    <div id="custom-file-container" hidden>
      <label id="attached-filename-text"></label>
      <button id="remove-attached-file" class="remove-file-button"></button>
    </div>
    <div id="attach-error" class="attach-file-notification"
          role="alert" hidden>$i18n{attachFileToBig}</div>
  </div>
  <div id="attach-file-note" aria-hidden="true">$i18n{attachFileNote}</div>
  <!-- User Consent -->
  <div id="consent-container" class="checkbox-field-container" hidden>
    <input id="consent-checkbox" type="checkbox"
        aria-labelledby="consent-chk-label">
    <label id="consent-chk-label">$i18n{consentCheckboxLabel}</label>
  </div>
  <!-- Offensive or unsafe -->
  <div id="offensive-container" class="checkbox-field-container" hidden>
    <input id="offensive-checkbox" type="checkbox"
        aria-labelledby="offensive-checkbox-label">
    <label id="offensive-checkbox-label">$i18n{offensiveCheckboxLabel}</label>
  </div>
  <!-- Show log id -->
  <div id="log-id-container" class="checkbox-field-container" hidden>
    <input id="log-id-checkbox" type="checkbox"
        aria-labelledby="log-id-checkbox-label" checked>
    <label id="log-id-checkbox-label">$i18n{logIdCheckboxLabel}</label>
  </div>
  <!-- Screenshot -->
  <div id="screenshot-container" class="checkbox-field-container">
    <input id="screenshot-checkbox" type="checkbox"
            aria-labelledby="screenshot-chk-label">
    <label id="screenshot-chk-label">$i18n{screenshot}</label>
    <img id="screenshot-image" aria-label="$i18n{screenshotA11y}">
  </div>
  <!-- Autofill Metadata (Googler Internal Only) -->
  <div id="autofill-checkbox-container"
        class="checkbox-field-container" hidden>
    <input id="autofill-metadata-checkbox" type="checkbox" checked>
    <label id="autofill-metadata-label">$i18nRaw{autofillMetadataInfo}</label>
  </div>
  <!-- System Information -->
  <div id="sys-info-container" class="checkbox-field-container">
    <input id="sys-info-checkbox" type="checkbox"
            aria-labelledby="sys-info-label" checked>
    <label id="sys-info-label">$i18nRaw{sysInfo}</label>
  </div>
  <!-- Privacy note -->
  <div id="privacy-note" class="privacy-note">$i18nRaw{privacyNote}</div>
<if expr="chromeos_ash">
  <div id="share-privacy-note" class="privacy-note">
    $i18n{mayBeSharedWithPartnerNote}
  </div>
</if>
</div>
<!-- Buttons -->
<div id="bottom-buttons-container" class="content">
  <div class="buttons-pane bottom-buttons">
    <button id="cancel-button" type="submit" class="white-button">
      $i18n{cancel}
<if expr="chromeos_ash">
  <div id="cancel-button-hover-bg"></div>
</if>
    </button>
    <button id="send-report-button" type="submit"
        class="blue-button" aria-describedby="questionnaire-notification">
      $i18n{sendReport}
<if expr="chromeos_ash">
  <div id="send-button-hover-bg"></div>
</if>
    </button>
  </div>
</div>
<!--_html_template_end_-->`;
}
