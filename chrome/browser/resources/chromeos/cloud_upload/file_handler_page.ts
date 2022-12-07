// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import {assert} from 'chrome://resources/js/assert_ts.js';

import {UserAction} from './cloud_upload.mojom-webui.js';
import {CloudUploadBrowserProxy} from './cloud_upload_browser_proxy.js';
import {getTemplate} from './file_handler_page.html.js';

/**
 * The FileHandlerPageElement represents the setup page the user sees after
 * choosing Docs/Sheets/Slides.
 */
export class FileHandlerPageElement extends HTMLElement {
  private proxy: CloudUploadBrowserProxy =
      CloudUploadBrowserProxy.getInstance();

  constructor() {
    super();
    const shadowRoot = this.attachShadow({mode: 'open'});

    shadowRoot.innerHTML = getTemplate();
    const openButton = shadowRoot.querySelector<HTMLElement>('.action-button');
    const cancelButton =
        shadowRoot.querySelector<HTMLElement>('.cancel-button');
    assert(openButton);
    assert(cancelButton);

    openButton.addEventListener('click', () => this.onOpenButtonClick());
    cancelButton.addEventListener('click', () => this.onCancelButtonClick());

    this.initDynamicContent();
  }

  // Sets the dynamic content of the page like the file name.
  async initDynamicContent() {
    try {
      const [dialogArgs, {installed: isOfficePwaInstalled}] =
          await Promise.all([
            this.proxy.handler.getDialogArgs(),
            this.proxy.handler.isOfficeWebAppInstalled(),
          ]);
      assert(dialogArgs.args);

      if (isOfficePwaInstalled) {
        this.shadowRoot!.querySelector('#available-to-install')!.remove();
      }

      const fileNameElement =
          this.shadowRoot!.querySelector<HTMLSpanElement>('#file-name');
      assert(fileNameElement);
      fileNameElement.innerText = dialogArgs.args.fileNames[0] || '';

      const {name, icon} = this.getDriveAppInfo(dialogArgs.args.fileNames);
      const driveAppNameElement =
          this.shadowRoot!.querySelector<HTMLSpanElement>('#drive-app-name');
      assert(driveAppNameElement);
      driveAppNameElement.innerText = name;

      const driveAppIconElement =
          this.shadowRoot!.querySelector<HTMLSpanElement>('#drive-app-icon');
      assert(driveAppIconElement);
      driveAppIconElement.classList.add(icon);


    } catch (e) {
      // TODO(b:243095484) Define expected behavior.
      console.error(`Unable to get dialog arguments . Error: ${e}.`);
    }
  }

  // Return the name and icon of the specific Google app i.e. Docs/Sheets/Slides
  // that will be used to open these files. When there are multiple files of
  // different types, or any error finding the right app, we just default to
  // Docs.
  private getDriveAppInfo(fileNames: string[]) {
    // TODO(b:254586358): i18n these names.
    const fileName = fileNames[0] || '';
    if (/\.xlsx?$/.test(fileName)) {
      return {name: 'Google Sheets', icon: 'sheets'};
    } else if (/\.pptx?$/.test(fileName)) {
      return {name: 'Google Slides', icon: 'slides'};
    } else {
      return {name: 'Google Docs', icon: 'docs'};
    }
  }

  private getUserChoice() {
    if (this.shadowRoot!.querySelector<HTMLInputElement>('#drive')!.checked) {
      // TODO(petermarshall): Remove the kSetUpGoogleDrive step or use it here.
      return UserAction.kConfirmOrUploadToGoogleDrive;
    } else {
      return UserAction.kSetUpOneDrive;
    }
  }

  private onOpenButtonClick(): void {
    this.proxy.handler.respondAndClose(this.getUserChoice());
  }

  private onCancelButtonClick(): void {
    this.proxy.handler.respondAndClose(UserAction.kCancel);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'file-handler-page': FileHandlerPageElement;
  }
}

customElements.define('file-handler-page', FileHandlerPageElement);
