// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsDownloadsPageElement} from './downloads_page.js';

export function getHtml(this: SettingsDownloadsPageElement) {
  return html`<!--_html_template_start_-->
<settings-section page-title="$i18n{downloadsPageTitle}"
    class="cr-centered-card-container">
  <div class="cr-row first">
    <div class="flex cr-padded-text">
      <div id="locationLabel" aria-hidden="true">
        $i18n{downloadLocation}
      </div>
      <div class="secondary" id="defaultDownloadPath" aria-hidden="true">
<if expr="not is_chromeos">
        ${this.downloadDefaultDirectoryPref_?.value}
</if>
<if expr="is_chromeos">
        ${this.downloadLocation_}
</if>
      </div>
    </div>
    <div class="separator"></div>
    <controlled-button id="changeDownloadsPath"
        label="$i18n{changeDownloadLocation}"
        aria-labelledby="locationLabel defaultDownloadPath"
        @click="${this.onChangeDownloadsPathClick_}"
        pref-key="download.default_directory"
        end-justified>
    </controlled-button>
  </div>
  <settings-toggle-button
      class="hr"
      pref-key="download.prompt_for_download"
      label="$i18n{promptForDownload}">
  </settings-toggle-button>
  ${this.autoOpenDownloads_ ? html`
    <div class="cr-row">
      <div class="flex">$i18n{openFileTypesAutomatically}</div>
      <div class="separator"></div>
      <cr-button id="resetAutoOpenFileTypes"
          @click="${this.onClearAutoOpenFileTypesClick_}">
        $i18n{clear}
      </cr-button>
    </div>
  ` : ''}
  ${this.downloadBubblePartialViewControlledByPref_ ? html`
    <settings-toggle-button id="showDownloadsToggle"
        class="hr"
        pref-key="download_bubble.partial_view_enabled"
        label="$i18n{showDownloadsWhenFinished}">
    </settings-toggle-button>
  ` : ''}
</settings-section>
<!--_html_template_end_-->`;
}
