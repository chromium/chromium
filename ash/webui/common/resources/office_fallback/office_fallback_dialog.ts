// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import {assert} from 'chrome://resources/js/assert_ts.js';

import {DialogChoice} from './office_fallback.mojom-webui.js';
import {OfficeFallbackBrowserProxy} from './office_fallback_browser_proxy.js';
import {getTemplate} from './office_fallback_dialog.html.js';

// The enum should be consistent with
// ash::office_fallback::FallbackReason
enum FallbackReason {
  OFFLINE = 0,
  DRIVE_UNAVAILABLE = 1,
  ONEDRIVE_UNAVAILABLE = 2,
  ERROR_OPENING_WEB = 3,
}

/**
 * The OfficeFallbackElement represents the dialog that allows the user to
 * choose what to do when failing to open office files.
 * @extends HTMLElement
 */
export class OfficeFallbackElement extends HTMLElement {
  /** Array of file names representing the files selected by the user. */
  fileNames: string[] = [];
  /** Title of the task that failed to open the files. */
  taskTitle: string = '';
  fallbackReason: FallbackReason|undefined;
  private root: ShadowRoot;

  constructor() {
    super();
    this.processDialogArgs();
    const template = this.createTemplate();
    const fragment = template.content.cloneNode(true);
    this.root = this.attachShadow({mode: 'open'});
    this.root.appendChild(fragment);
  }

  $<T extends HTMLElement>(query: string): T {
    return this.root.querySelector(query)!;
  }

  get proxy() {
    return OfficeFallbackBrowserProxy.getInstance();
  }

  async connectedCallback() {
    const quickOfficeButton = this.$('#quick-office-button')!;
    const tryAgainButton = this.$('#try-again-button')!;
    const cancelButton = this.$('#cancel-button')!;
    quickOfficeButton.addEventListener(
        'click', () => this.onQuickOfficeButtonClick());
    tryAgainButton.addEventListener(
        'click', () => this.onTryAgainButtonClick());
    cancelButton.addEventListener('click', () => this.onCancelButtonClick());
  }

  // The mapping should be consistent with
  // ash::office_fallback::FallbackReasonToString()
  stringToFallbackReason(fallbackReason: string): FallbackReason|undefined {
    switch (fallbackReason) {
      case 'Offline':
        return FallbackReason.OFFLINE;
      case 'Drive Unavailable':
        return FallbackReason.DRIVE_UNAVAILABLE;
      case 'OneDrive Unavailable':
        return FallbackReason.ONEDRIVE_UNAVAILABLE;
      case 'Error opening web':
        return FallbackReason.ERROR_OPENING_WEB;
    }
    console.error('No matching FallbackReason for given string');
    return;
  }

  /**
   * Initialises the class members based off the given dialog arguments.
   */
  processDialogArgs() {
    try {
      const dialogArgs = this.proxy.getDialogArguments();
      assert(dialogArgs);
      const args = JSON.parse(dialogArgs);
      assert(args);
      assert(args.fileNames);
      assert(args.taskTitle);
      assert(args.fallbackReason);
      this.fileNames = args.fileNames;
      this.taskTitle = args.taskTitle;
      this.fallbackReason = this.stringToFallbackReason(args.fallbackReason);
    } catch (e) {
      console.error(`Unable to get dialog arguments. Error: ${e}.`);
    }
  }

  createTemplate(): HTMLTemplateElement {
    const template = document.createElement('template');
    template.innerHTML = getTemplate() as string;
    const fragment = template.content;
    const titleElement = fragment.querySelector('#title')! as HTMLElement;
    const reasonMessageElement =
        fragment.querySelector('#reason-message')! as HTMLElement;
    const optionsMessageElement =
        fragment.querySelector('#options-message')! as HTMLElement;

    // TODO(cassycc): replace with UX chosen text.
    // TODO(b/242685536) When multi-file selection is defined, implement
    // `fileNamesDisplayed` appropriately. Currently, fileNames is a singleton
    // array.
    // TODO(cassycc): Handle long file name(s).
    // TODO(cassycc): Translate the text based on the device's language.
    const fileNamesDisplayed =
        this.fileNames.map(name => `"${name}"`).join(', ');
    optionsMessageElement.innerText = `Choose "Try again", or choose \
          "Open in offline editor" to use limited view and editing options.`;
    switch (this.fallbackReason) {
      case FallbackReason.OFFLINE:
        titleElement.innerText =
            `Can't open ${fileNamesDisplayed} when offline`;
        reasonMessageElement.innerText =
            `The application ${this.taskTitle} isn’t available offline.`;
        optionsMessageElement.innerText =
            `Check your internet connection and choose "Try again", or choose \
            "Open in offline editor" to use limited view and editing options.`;
        break;
      case FallbackReason.DRIVE_UNAVAILABLE:
        titleElement.innerText =
            `Can't open ${fileNamesDisplayed} when Drive is not available`;
        reasonMessageElement.innerText =
            `The application ${this.taskTitle} requires Drive to be \
            available.`;
        break;
      case FallbackReason.ONEDRIVE_UNAVAILABLE:
        titleElement.innerText =
            `Can't open ${fileNamesDisplayed} when OneDrive is not available`;
        reasonMessageElement.innerText =
            `The application ${this.taskTitle} requires OneDrive \
          to be available.`;
        break;
      case FallbackReason.ERROR_OPENING_WEB:
        titleElement.innerText = `Can't open the URL for ${fileNamesDisplayed}`;
        reasonMessageElement.innerText =
            `The application ${this.taskTitle} requires OneDrive \
          to be available.`;
        break;
    }
    return template;
  }

  private onQuickOfficeButtonClick(): void {
    this.proxy.handler.close(DialogChoice.kQuickOffice);
  }

  private onTryAgainButtonClick(): void {
    this.proxy.handler.close(DialogChoice.kTryAgain);
  }

  private onCancelButtonClick(): void {
    this.proxy.handler.close(DialogChoice.kCancel);
  }
}

customElements.define('office-fallback', OfficeFallbackElement);