// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert_ts.js';

import {CANCEL_SETUP_EVENT, NEXT_PAGE_EVENT} from './base_setup_page.js';
import {UserAction} from './cloud_upload.mojom-webui.js';
import {CloudUploadBrowserProxy} from './cloud_upload_browser_proxy.js';
import {OneDriveUploadPageElement} from './one_drive_upload_page.js';
import {WelcomePageElement} from './welcome_page.js';

export enum UploadType {
  ONE_DRIVE = 0,
  DRIVE = 1,
}

/**
 * The CloudUploadElement is the main dialog controller that aggregates all the
 * individual setup pages and determines which one to show.
 */
export class CloudUploadElement extends HTMLElement {
  /** List of pages to show. */
  pages: HTMLElement[];
  /** The current page index into `pages`. */
  private currentPageIdx: number = 0;
  private fileName: string|null = null;

  constructor() {
    super();
    this.processDialogArgs();
    this.attachShadow({mode: 'open'});

    // TODO(b/251046341): Adjust this once the rest of the pages are in place.
    const oneDriveUploadPage = new OneDriveUploadPageElement();
    oneDriveUploadPage.setFileName(this.fileName);
    this.pages = [
      new WelcomePageElement(),
      oneDriveUploadPage,
    ];
    for (let i = 0; i < this.pages.length; i++) {
      this.pages[i]?.setAttribute(
          'total-pages', (this.pages.length).toString());
      this.pages[i]?.setAttribute('page-number', i.toString());
      this.pages[i]?.addEventListener(NEXT_PAGE_EVENT, () => this.goNextPage());
      this.pages[i]?.addEventListener(
          CANCEL_SETUP_EVENT, () => this.cancelSetup());
    }
    this.switchPage(0);
  }

  get proxy() {
    return CloudUploadBrowserProxy.getInstance();
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
    this.shadowRoot?.appendChild(this.currentPage!);
  }

  /**
   * Initialises the class members based off the given dialog arguments.
   */
  private processDialogArgs(): void {
    try {
      const dialogArgs = this.proxy.getDialogArguments();
      assert(dialogArgs);
      const args = JSON.parse(dialogArgs);
      assert(args);
      if (args.fileName != null) {
        this.fileName = args.fileName;
      }
    } catch (e) {
      // TODO(b/243095484) Define expected behavior.
      console.error(`Unable to get dialog arguments . Error: ${e}.`);
    }
  }

  /**
   * Invoked when a page fires a `CANCEL_SETUP_EVENT` event.
   */
  private cancelSetup(): void {
    this.proxy.handler.respondAndClose(UserAction.kCancel);
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

customElements.define('cloud-upload', CloudUploadElement);
