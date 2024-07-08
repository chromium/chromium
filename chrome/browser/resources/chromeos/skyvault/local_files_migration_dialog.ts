// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'local-files-migration-dialog' defines the UI for the SkyVault migration
 * workflow.
 */

import 'chrome://resources/ash/common/cr_elements/cros_color_overrides.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_page_host_style.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import './strings.m.js';

import {getTemplate} from './local_files_migration_dialog.html.js';

class LocalFilesMigrationDialogElement extends HTMLElement {
  constructor() {
    super();

    const shadowRoot = this.attachShadow({mode: 'open'});
    shadowRoot.innerHTML = getTemplate();

    const uploadNowButton = this.$('#upload-now-button');
    uploadNowButton.addEventListener(
        'click', () => this.onUploadNowButtonClicked_());
    const dismissButton = this.$('#dismiss-button');
    dismissButton.addEventListener(
        'click', () => this.onDismissButtonClicked_());
  }

  $<T extends HTMLElement>(query: string): T {
    return this.shadowRoot!.querySelector(query)!;
  }

  private async onUploadNowButtonClicked_() {
    chrome.send('startMigration');
  }

  private onDismissButtonClicked_() {
    chrome.send('dialogClose');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    ['local-files-migration-dialog']: LocalFilesMigrationDialogElement;
  }
}

customElements.define(
    'local-files-migration-dialog', LocalFilesMigrationDialogElement);
