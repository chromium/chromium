// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './setup_cancel_dialog.js';

import {assert} from 'chrome://resources/js/assert.js';

import {CANCEL_SETUP_EVENT, NEXT_PAGE_EVENT} from './base_setup_page.js';
import {MetricsRecordedSetupPage, UserAction} from './cloud_upload.mojom-webui.js';
import {CloudUploadBrowserProxy} from './cloud_upload_browser_proxy.js';
import {OfficePwaInstallPageElement} from './office_pwa_install_page.js';
import {OfficeSetupCompletePageElement} from './office_setup_complete_page.js';
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

  // Save reference to listener so it can be removed from the document in
  // disconnectedCallback().
  private boundKeyDownListener_: (e: KeyboardEvent) => void;

  /**
    True if the setup flow should end with setting Microsoft 365 as default
    handler. Note: This is usually done if no default file handlers have been
    set for Office files, which means that the setup flow is being completed for
    the first time.
  */
  private setOfficeAsDefaultHandler: boolean = true;

  /** The names of the files to upload. */
  private fileNames: string[] = [];

  constructor() {
    super();
    const shadow = this.attachShadow({mode: 'open'});

    this.cancelDialog = document.createElement('setup-cancel-dialog');
    shadow.appendChild(this.cancelDialog);

    this.boundKeyDownListener_ = this.onKeyDown.bind(this);

    this.initPromise = this.init();
  }

  connectedCallback(): void {
    document.addEventListener('keydown', this.boundKeyDownListener_);
  }

  disconnectedCallback(): void {
    document.removeEventListener('keydown', this.boundKeyDownListener_);
  }

  async init(): Promise<void> {
    const [, {installed: isOfficeWebAppInstalled}, {mounted: isOdfsMounted}] =
        await Promise.all([
          this.processDialogArgs(),
          this.proxy.handler.isOfficeWebAppInstalled(),
          this.proxy.handler.isODFSMounted(),
        ]);

    // Only skip this page if the setup flow is not run as part of the "file
    // upload" flow, and file handlers still need to be set.
    if (this.fileNames.length !== 0 || this.setOfficeAsDefaultHandler) {
      const welcomePage = new WelcomePageElement();
      welcomePage.setInstalled(isOfficeWebAppInstalled, isOdfsMounted);
      this.pages.push(welcomePage);
    }

    if (!isOfficeWebAppInstalled) {
      this.pages.push(new OfficePwaInstallPageElement());
    }

    if (!isOdfsMounted) {
      this.pages.push(new SignInPageElement());
    }

    const officeSetupCompletePage = new OfficeSetupCompletePageElement();
    officeSetupCompletePage.setDefaultHandlerOnPageShown(
        this.setOfficeAsDefaultHandler);
    this.pages.push(officeSetupCompletePage);

    for (const page of this.pages) {
      page.addEventListener(NEXT_PAGE_EVENT, () => this.goNextPage());
      page.addEventListener(CANCEL_SETUP_EVENT, () => this.cancelSetup());
    }

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
      assert(dialogArgs.args.dialogSpecificArgs.oneDriveSetupDialogArgs);
      this.setOfficeAsDefaultHandler =
          dialogArgs.args.dialogSpecificArgs.oneDriveSetupDialogArgs
              .setOfficeAsDefaultHandler;
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

  private currentPageToMetricsPage(): MetricsRecordedSetupPage|null {
    if (this.currentPage instanceof WelcomePageElement) {
      return MetricsRecordedSetupPage.kOneDriveSetupWelcome;
    } else if (this.currentPage instanceof OfficePwaInstallPageElement) {
      return MetricsRecordedSetupPage.kOneDriveSetupPWAInstall;
    } else if (this.currentPage instanceof SignInPageElement) {
      return MetricsRecordedSetupPage.kOneDriveSetupODFSMount;
    } else if (this.currentPage instanceof OfficeSetupCompletePageElement) {
      return MetricsRecordedSetupPage.kOneDriveSetupComplete;
    }
    return null;
  }

  /**
   * Invoked when a page fires a `CANCEL_SETUP_EVENT` event.
   */
  private cancelSetup(): void {
    if (this.currentPage instanceof OfficeSetupCompletePageElement) {
      // No need to show the cancel dialog as setup is finished.
      this.proxy.handler.respondWithUserActionAndClose(UserAction.kCancel);
      return;
    }
    this.cancelDialog.show(() => {
      const metricsPage = this.currentPageToMetricsPage();
      if (metricsPage != null) {
        this.proxy.handler.recordCancel(metricsPage);
      }
      this.proxy.handler.respondWithUserActionAndClose(UserAction.kCancel);
    });
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
