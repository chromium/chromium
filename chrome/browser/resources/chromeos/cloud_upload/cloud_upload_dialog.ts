// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './setup_cancel_dialog.js';

import {assert} from 'chrome://resources/js/assert_ts.js';

import {CANCEL_SETUP_EVENT, NEXT_PAGE_EVENT} from './base_setup_page.js';
import {UserAction} from './cloud_upload.mojom-webui.js';
import {CloudUploadBrowserProxy} from './cloud_upload_browser_proxy.js';
import {OfficePwaInstallPageElement} from './office_pwa_install_page.js';
import {OneDriveUploadPageElement} from './one_drive_upload_page.js';
import type {SetupCancelDialogElement} from './setup_cancel_dialog.js';
import {SignInPageElement} from './sign_in_page.js';
import {WelcomePageElement} from './welcome_page.js';

/**
 * The CloudUploadElement is the main dialog controller that aggregates all the
 * individual setup pages and determines which one to show.
 */
export class CloudUploadElement extends HTMLElement {
  private proxy = CloudUploadBrowserProxy.getInstance();

  /** Resolved once the element's shadow DOM has finished initializing. */
  initPromise: Promise<void>;

  /** List of pages to show. */
  pages: HTMLElement[] = [];

  /** The current page index into `pages`. */
  private currentPageIdx: number = 0;

  /** The modal dialog shown to confirm if the user wants to cancel setup. */
  private cancelDialog: SetupCancelDialogElement;

  /**
    True if the setup flow is being run for the first time. False if the fixup
    flow is being run.
  */
  private firstTimeSetup: boolean = true;

  /** The names of the files to upload. */
  private fileNames: string[] = [];

  constructor() {
    super();
    const shadow = this.attachShadow({mode: 'open'});

    this.cancelDialog = document.createElement('setup-cancel-dialog');
    shadow.appendChild(this.cancelDialog);

    document.addEventListener('keydown', this.onKeyDown.bind(this));

    this.initPromise = this.init();
  }

  async init(): Promise<void> {
    const [, {installed: isOfficeWebAppInstalled}, {mounted: isOdfsMounted}] =
        await Promise.all([
          this.processDialogArgs(),
          this.proxy.handler.isOfficeWebAppInstalled(),
          this.proxy.handler.isODFSMounted(),
        ]);

    // TODO(b/251046341): Adjust this once the rest of the pages are in place.
    const welcomePage = new WelcomePageElement();
    welcomePage.setInstalled(isOfficeWebAppInstalled, isOdfsMounted);
    this.pages.push(welcomePage);

    if (!isOfficeWebAppInstalled) {
      this.pages.push(new OfficePwaInstallPageElement());
    }

    if (!isOdfsMounted) {
      this.pages.push(new SignInPageElement());
    }

    const oneDriveUploadPage = new OneDriveUploadPageElement();
    oneDriveUploadPage.setFileNamesAndFirstTimeSetup(
        this.fileNames, this.firstTimeSetup);
    this.pages.push(oneDriveUploadPage);

    this.pages.forEach((page, index) => {
      page.setAttribute('total-pages', String(this.pages.length));
      page.setAttribute('page-number', String(index));
      page.addEventListener(NEXT_PAGE_EVENT, () => this.goNextPage());
      page.addEventListener(CANCEL_SETUP_EVENT, () => this.cancelSetup());
    });

    this.switchPage(0);
  }

  $<T extends HTMLElement>(query: string): T {
    return this.shadowRoot!.querySelector(query)!;
  }

  /**
   * Gets the element corresponding to the currently shown page.
   */
  get currentPage(): HTMLElement|undefined {
    return this.pages[this.currentPageIdx];
  }

  /**
   * Switches the currently shown page.
   * @param page Page index to show.
   */
  private switchPage(page: number): void {
    this.currentPage?.remove();
    this.currentPageIdx = page;
    this.shadowRoot!.appendChild(this.currentPage!);
  }

  /**
   * Initialises the class members based off the given dialog arguments.
   */
  private async processDialogArgs(): Promise<void> {
    try {
      const dialogArgs = await this.proxy.handler.getDialogArgs();
      assert(dialogArgs.args);
      this.firstTimeSetup = dialogArgs.args.firstTimeSetup;
      this.fileNames = dialogArgs.args.fileNames;
    } catch (e) {
      // TODO(b/243095484) Define expected behavior.
      console.error(`Unable to get dialog arguments . Error: ${e}.`);
    }
  }

  private onKeyDown(event: KeyboardEvent) {
    if (event.key === 'Escape' && !this.cancelDialog.open) {
      this.cancelSetup();
      // Stop escape from also immediately closing the dialog.
      event.stopImmediatePropagation();
      event.preventDefault();
    }
  }

  /**
   * Invoked when a page fires a `CANCEL_SETUP_EVENT` event.
   */
  private cancelSetup(): void {
    if (this.currentPage instanceof OneDriveUploadPageElement) {
      // No need to show the cancel dialog as setup is finished.
      this.proxy.handler.respondWithUserActionAndClose(UserAction.kCancel);
      return;
    }
    this.cancelDialog.show(
        () => this.proxy.handler.respondWithUserActionAndClose(
            UserAction.kCancel));
  }

  /**
   * Invoked when a page fires a `NEXT_PAGE_EVENT` event.
   */
  private goNextPage(): void {
    if (this.currentPageIdx < this.pages.length - 1) {
      this.switchPage(this.currentPageIdx + 1);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cloud-upload': CloudUploadElement;
  }
}

customElements.define('cloud-upload', CloudUploadElement);
