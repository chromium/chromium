// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import {assert} from 'chrome://resources/js/assert_ts.js';

import {DialogTask, UserAction} from './cloud_upload.mojom-webui.js';
import {CloudUploadBrowserProxy} from './cloud_upload_browser_proxy.js';
import {getTemplate} from './file_handler_page.html.js';

/**
 * The FileHandlerPageElement represents the setup page the user sees after
 * choosing Docs/Sheets/Slides or the Office PWA.
 */
export class FileHandlerPageElement extends HTMLElement {
  /**
   * The local file tasks that the user could use to open the file. There are
   * separate buttons for the Drive and Office PWA apps.
   */
  tasks: DialogTask[] = [];
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
      assert(dialogArgs.args.tasks);

      // Element which marks the start of the section of uninstalled apps.
      const uninstalledApps = this.$('#available-to-install');

      const fileNameElement = this.$<HTMLSpanElement>('#file-name');
      assert(fileNameElement);
      fileNameElement.innerText = dialogArgs.args.fileNames[0] || '';

      const {name, icon} = this.getDriveAppInfo(dialogArgs.args.fileNames);
      const driveAppNameElement = this.$<HTMLSpanElement>('#drive-app-name');
      assert(driveAppNameElement);
      driveAppNameElement.innerText = name;

      const driveAppIconElement = this.$<HTMLSpanElement>('#drive-app-icon');
      assert(driveAppIconElement);
      driveAppIconElement.classList.add(icon);

      const form = this.$<HTMLSpanElement>('form');
      // For each local file task, create a clickable label.
      for (const task of dialogArgs.args.tasks) {
        assert(task);

        const label = document.createElement('label');
        label.className = 'radio-label';

        const input = document.createElement('input');
        input.type = 'radio';
        input.name = 'app-choice';
        // Expect the position to be positive.
        assert(task.position >= 0);
        input.id = this.toStringId(task.position);

        const div = document.createElement('div');
        div.className = 'icon start';
        div.setAttribute(
            'style', 'background-image: url(' + task.iconUrl + ')');

        const span = document.createElement('span');
        span.innerText = task.title;

        label.append(input);
        label.append(div);
        label.append(span);
        // Put the label in the installed apps section.
        form!.insertBefore(label, uninstalledApps!);
      }

      // Remove the text for the section of uninstalled apps if the Office PWA,
      // the only uninstalled app, is already installed.
      // TODO(cassycc): Check if the Office PWA should be in the at the top,
      // below Drive, in this case.
      if (isOfficePwaInstalled) {
        uninstalledApps!.remove();
      }

      // Set `this.tasks` at end of `initDynamicContent` as an indication of
      // completion.
      this.tasks = dialogArgs.args.tasks;

    } catch (e) {
      // TODO(b:243095484) Define expected behavior.
      console.error(`Unable to get dialog arguments . Error: ${e}.`);
    }
  }

  $<T extends HTMLElement>(query: string): T {
    return this.shadowRoot!.querySelector(query)!;
  }

  // Convert a number to a string that can be used as an id for an element. Add
  // the prefix 'id' so it can be found with the `querySelector`.
  private toStringId(i: number): string {
    return 'id' + i;
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

  // If the user clicked on the Drive or Office PWA app return the corresponding
  // `UserAction`. Otherwise return null.
  private getSelectedCloudProvider() {
    if (this.$<HTMLInputElement>('#drive')!.checked) {
      // TODO(petermarshall): Remove the kSetUpGoogleDrive step or use it here.
      return UserAction.kConfirmOrUploadToGoogleDrive;
    } else if (this.$<HTMLInputElement>('#onedrive')!.checked) {
      return UserAction.kSetUpOneDrive;
    } else {
      return null;
    }
  }

  // Return the (positive) id of the local file task clicked on by the user. If
  // not found, return -1.
  private getSelectedLocalTask(): number {
    for (const task of this.tasks) {
      if (this.shadowRoot!
              .querySelector<HTMLInputElement>(
                  '#' + this.toStringId(task.position))!.checked) {
        return task.position;
      }
    }
    console.error('Unable to get selected local file task.');
    return -1;
  }

  // Invoked when the open file button is clicked. If the user previously
  // clicked on the Drive or Office PWA app, trigger the right
  // `respondWithUserActionAndClose` mojo request. If the user previously
  // clicked on a local file task, trigger the right
  // `respondWithLocalTaskAndClose` mojo request.
  private onOpenButtonClick(): void {
    const userChoice = this.getSelectedCloudProvider();
    if (userChoice) {
      this.proxy.handler.respondWithUserActionAndClose(userChoice);
    } else {
      // TODO(cassycc): It is possible that the click can happen before
      // initDynamicContent (as I found in my tests). Should we be enforcing
      // ordering? Would adding an await solve it?
      const taskPosition = this.getSelectedLocalTask();
      if (0 <= taskPosition) {
        this.proxy.handler.respondWithLocalTaskAndClose(taskPosition);
      }
    }
  }


  private onCancelButtonClick(): void {
    this.proxy.handler.respondWithUserActionAndClose(UserAction.kCancel);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'file-handler-page': FileHandlerPageElement;
  }
}

customElements.define('file-handler-page', FileHandlerPageElement);
