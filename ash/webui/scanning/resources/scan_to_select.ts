// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './scan_settings_section.js';
import './strings.m.js';

import {I18nMixin, I18nMixinInterface} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './scan_to_select.html.js';
import {ScanningBrowserProxyImpl, SelectedPath} from './scanning_browser_proxy.js';

/**
 * @fileoverview
 * 'scan-to-select' displays the chosen directory to save completed scans.
 */

const ScanToSelectElementBase =
    I18nMixin(PolymerElement) as {new (): PolymerElement & I18nMixinInterface};

export class ScanToSelectElement extends ScanToSelectElementBase {
  static get is() {
    return 'scan-to-select' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      disabled: Boolean,

      /**
       * The lowest level directory in |selectedFilePath|.
       */
      selectedFolder: {
        type: String,
        notify: true,
      },

      selectedFilePath: {
        type: String,
        notify: true,
      },
    };
  }

  disabled: boolean;
  selectedFolder: string;
  selectedFilePath: string;
  private browserProxy = ScanningBrowserProxyImpl.getInstance();

  constructor() {
    super();

    // Default option is 'My files'.
    this.selectedFolder = this.i18n('myFilesSelectOption');

    this.browserProxy.initialize();
  }

  /**
   * Opens the select dialog and updates the dropdown to the user's selected
   * directory.
   */
  private onSelectFolder(): void {
    this.browserProxy.requestScanToLocation().then(
        (selectedPath: SelectedPath): void => {
          // When the select dialog closes, set dropdown back to
          // |selectedFolder| option.
          strictQuery('#scanToSelect', this.shadowRoot, HTMLSelectElement)
              .selectedIndex = 0;

          const baseName = selectedPath.baseName;
          const filePath = selectedPath.filePath;
          // When the select dialog is canceled, |baseName| and |filePath| will
          // be empty.
          if (!baseName || !filePath) {
            return;
          }

          this.selectedFolder = baseName;
          this.selectedFilePath = filePath;
        });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [ScanToSelectElement.is]: ScanToSelectElement;
  }
}

customElements.define(ScanToSelectElement.is, ScanToSelectElement);
