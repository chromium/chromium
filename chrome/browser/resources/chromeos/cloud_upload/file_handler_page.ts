// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {DialogTask, MetricsRecordedSetupPage, UserAction} from './cloud_upload.mojom-webui.js';
import {CloudUploadBrowserProxy} from './cloud_upload_browser_proxy.js';
import {AccordionTopCardElement, BaseCardElement, CloudProviderCardElement, CloudProviderType, FileHandlerCardElement, LocalHandlerCardElement} from './file_handler_card.js';
import {getTemplate} from './file_handler_page.html.js';

/**
 * The FileHandlerPageElement represents the setup page the user sees to select
 * the their preferred office file handler: Docs/Sheets/Slides, the Office PWA
 * or a local task.
 */
export class FileHandlerPageElement extends HTMLElement {
  /**
   * The local file tasks that the user could use to open the file. There are
   * separate buttons for the Drive and Office PWA apps.
   */
  localTasks: DialogTask[] = [];
  /**
   * References to the HTMLElement used to display the tasks that the user can
   * select.
   */
  cloudProviderCards: CloudProviderCardElement[] = [];
  localHandlerCards: LocalHandlerCardElement[] = [];
  cards: BaseCardElement[] = [];

  private proxy: CloudUploadBrowserProxy =
      CloudUploadBrowserProxy.getInstance();

  // Save reference to listener so it can be removed from the document in
  // disconnectedCallback().
  private boundKeyDownListener_: (e: KeyboardEvent) => void;

  constructor() {
    super();
    const shadowRoot = this.attachShadow({mode: 'open'});

    shadowRoot.innerHTML = getTemplate();
    const openButton = this.$<CrButtonElement>('.action-button');
    const cancelButton = this.$<CrButtonElement>('.cancel-button');
    assert(openButton);
    assert(cancelButton);

    openButton.disabled = true;
    openButton.addEventListener('click', () => this.onOpenButtonClick());
    cancelButton.addEventListener('click', () => this.onCancelButtonClick());
    this.boundKeyDownListener_ = this.handleKeyDown.bind(this);

    this.initDynamicContent();
  }

  // Initialises the scrollable content styles and add document event listeners.
  connectedCallback(): void {
    const contentElement = this.$<HTMLElement>('#content')!;
    window.requestAnimationFrame(() => {
      this.updateContentFade(contentElement);
    });
    contentElement.addEventListener(
        'scroll', this.updateContentFade.bind(undefined, contentElement),
        {passive: true});
    contentElement.addEventListener('keydown', this.boundKeyDownListener_);

    document.addEventListener('keydown', this.boundKeyDownListener_);
  }

  // Remove document event listeners.
  disconnectedCallback(): void {
    document.removeEventListener('keydown', this.boundKeyDownListener_);
  }

  $<T extends HTMLElement>(query: string): T {
    return this.shadowRoot!.querySelector(query)!;
  }

  // Sets the dynamic content of the page like the file name.
  async initDynamicContent() {
    try {
      const dialogArgs = await this.proxy.handler.getDialogArgs();
      assert(dialogArgs.args);
      assert(dialogArgs.args.dialogSpecificArgs);
      assert(dialogArgs.args.dialogSpecificArgs.fileHandlerDialogArgs);

      const fileHandlerDialogArgs =
          dialogArgs.args.dialogSpecificArgs.fileHandlerDialogArgs;

      const localTasks = fileHandlerDialogArgs?.localTasks;
      const showGoogleWorkspaceTask =
          fileHandlerDialogArgs?.showGoogleWorkspaceTask;
      const showMicrosoftOfficeTask =
          fileHandlerDialogArgs?.showMicrosoftOfficeTask;

      // Adjust the dialog's size if there are no local tasks to display.
      if (localTasks.length === 0) {
        this.$('#dialog').style.height = '315px';
      } else if (!showGoogleWorkspaceTask || !showMicrosoftOfficeTask) {
        this.$('#dialog').style.height = '295px';
      }

      const {name, icon, type} =
          this.getDriveAppInfo(dialogArgs.args.fileNames);

      const titleElement = this.$<HTMLSpanElement>('#title');
      assert(titleElement);
      titleElement.innerText =
          loadTimeData.getStringF('fileHandlerTitle', type);

      if (showGoogleWorkspaceTask) {
        const driveCard = new CloudProviderCardElement();
        driveCard.setParameters(
            CloudProviderType.DRIVE, name,
            loadTimeData.getString('googleDriveStorage'));
        driveCard.setIconClass(icon);
        driveCard.id = 'drive';
        this.addCloudProviderCard(driveCard);
      }

      if (showMicrosoftOfficeTask) {
        const officeCard = new CloudProviderCardElement();
        officeCard.setParameters(
            CloudProviderType.ONE_DRIVE, loadTimeData.getString('microsoft365'),
            loadTimeData.getString('oneDriveStorage'));
        officeCard.setIconClass('office');
        officeCard.id = 'onedrive';
        this.addCloudProviderCard(officeCard);
      }

      if (localTasks.length === 0) {
        return;
      }

      const accordionTopCard = new AccordionTopCardElement();
      accordionTopCard.id = 'accordion';
      this.addTopAccordionCard(accordionTopCard);

      // For each local file task, create a clickable label.
      for (let i = 0; i < localTasks.length; ++i) {
        const task = localTasks[i]!;
        assert(task);
        const localHandlerCard = new LocalHandlerCardElement();
        localHandlerCard.setParameters(task.position, task.title);
        localHandlerCard.setIconUrl(task.iconUrl);
        localHandlerCard.id = this.toStringId(task.position);
        if (i === localTasks.length - 1) {
          // Round bottom for last card.
          localHandlerCard.$('#container')!.classList.add('round-bottom');
        }
        this.addLocalHandlerCard(localHandlerCard);
      }
      // Set local tasks to indicate completion (used in tests).
      this.localTasks = localTasks;
    } catch (e) {
      // TODO(b:243095484) Define expected behavior.
      console.error(
          `Error while initialising dynamic content from dialog args: ${e}.`);
    }
  }

  addCloudProviderCard(providerCard: CloudProviderCardElement) {
    this.cloudProviderCards.push(providerCard);
    this.cards.push(providerCard);
    this.$<HTMLDivElement>('#content').appendChild(providerCard);
    providerCard.addEventListener('click', () => this.selectCard(providerCard));
  }

  addTopAccordionCard(topCard: AccordionTopCardElement) {
    this.$<HTMLDivElement>('#content').appendChild(topCard);
    this.cards.push(topCard);
    topCard.addEventListener('click', () => {
      const expanded = topCard.toggleExpandedState();
      for (const localhandlerCard of this.localHandlerCards) {
        if (expanded) {
          localhandlerCard.show();
        } else {
          localhandlerCard.hide();
          // Unselect any selected local handler and update action button.
          if (localhandlerCard.selected) {
            localhandlerCard.updateSelection(false);
            this.$<CrButtonElement>('.action-button').disabled = true;
          }
        }
      }
      const contentElement = this.$<HTMLDivElement>('#content')!;
      if (expanded) {
        window.requestAnimationFrame(() => {
          // Scroll so that the top of the accordion aligns to where the top of
          // the scrollable content is without scrolling.
          contentElement.scrollTop =
              topCard.offsetTop - contentElement.offsetTop;
          this.updateContentFade(contentElement);
        });
      } else {
        this.updateContentFade(contentElement);
      }
    });
  }

  addLocalHandlerCard(localHandlerCard: LocalHandlerCardElement) {
    localHandlerCard.hide();
    this.localHandlerCards.push(localHandlerCard);
    this.cards.push(localHandlerCard);
    this.$<HTMLDivElement>('#content').appendChild(localHandlerCard);
    localHandlerCard.addEventListener(
        'click', () => this.selectCard(localHandlerCard));
  }

  private selectCard(card: FileHandlerCardElement) {
    assert(card.style.display !== 'none', 'Attempting to select a hidden card');
    for (const providerCard of this.cloudProviderCards) {
      providerCard.updateSelection(providerCard === card);
    }
    for (const localHandlerCard of this.localHandlerCards) {
      localHandlerCard.updateSelection(localHandlerCard === card);
    }
    // Enable action button.
    if (card?.selected) {
      this.$<CrButtonElement>('.action-button').disabled = false;
    }
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
    const fileName = (fileNames[0] || '').toLowerCase();
    if (/\.xls[m,x]?$/.test(fileName)) {
      return {
        name: loadTimeData.getString('googleSheets'),
        icon: 'sheets',
        type: loadTimeData.getString('excel'),
      };
    } else if (/\.pptx?$/.test(fileName)) {
      return {
        name: loadTimeData.getString('googleSlides'),
        icon: 'slides',
        type: loadTimeData.getString('powerPoint'),
      };
    } else {
      return {
        name: loadTimeData.getString('googleDocs'),
        icon: 'docs',
        type: loadTimeData.getString('word'),
      };
    }
  }

  handleKeyDown(e: KeyboardEvent): void {
    if (e.key === 'Escape') {
      // Handle Escape as a "cancel".
      e.stopImmediatePropagation();
      e.preventDefault();
      this.onCancelButtonClick();
      return;
    }

    // Prevent scroll on spacebar.
    if (e.key === ' ') {
      e.preventDefault();
      return;
    }

    // Move card focus with arrow keys.
    let direction = 0;
    if (e.key === 'ArrowDown') {
      direction = 1;
    } else if (e.key === 'ArrowUp') {
      direction = -1;
    } else {
      return;
    }

    let selectedIndex = -1;
    for (let i = 0; i < this.cards.length; ++i) {
      if (this.cards[i] === this.shadowRoot!.activeElement) {
        selectedIndex = i;
      }
    }

    // If no card is focused, select the first one.
    if (selectedIndex === -1) {
      this.cards[0]!.focus();
      return;
    }

    const newSelectedIndex = selectedIndex + direction;
    if (newSelectedIndex < 0 || newSelectedIndex > this.cards.length - 1 ||
        this.cards[newSelectedIndex]?.style.display === 'none') {
      return;
    }
    this.cards[newSelectedIndex]!.focus();
  }

  // Invoked when the open file button is clicked. If the user previously
  // clicked on the Drive or Office PWA app, trigger the right
  // `respondWithUserActionAndClose` mojo request. If the user previously
  // clicked on a local file task, trigger the right
  // `respondWithLocalTaskAndClose` mojo request.
  private onOpenButtonClick(): void {
    if (this.$<CrButtonElement>('.action-button').disabled) {
      return;
    }

    for (const providerCard of this.cloudProviderCards) {
      if (!providerCard.selected) {
        continue;
      }
      if (providerCard.type === CloudProviderType.DRIVE) {
        this.proxy.handler.respondWithUserActionAndClose(
            UserAction.kConfirmOrUploadToGoogleDrive);
        return;
      } else if (providerCard.type === CloudProviderType.ONE_DRIVE) {
        this.proxy.handler.respondWithUserActionAndClose(
            UserAction.kSetUpOneDrive);
        return;
      }
    }
    for (const localHandlerCard of this.localHandlerCards) {
      if (!localHandlerCard.selected) {
        continue;
      }
      if (localHandlerCard.taskPosition >= 0) {
        this.proxy.handler.respondWithLocalTaskAndClose(
            localHandlerCard.taskPosition);
        return;
      }
    }

    assertNotReached('Unable to get selected task.');
  }

  private onCancelButtonClick(): void {
    this.proxy.handler.recordCancel(MetricsRecordedSetupPage.kFileHandlerPage);
    this.proxy.handler.respondWithUserActionAndClose(UserAction.kCancel);
  }

  private updateContentFade(contentElement: HTMLElement): void {
    window.requestAnimationFrame(() => {
      const atTop = contentElement.scrollTop === 0;
      const atBottom =
          Math.abs(
              contentElement.scrollHeight - contentElement.clientHeight -
              contentElement.scrollTop) < 1;
      contentElement.classList.toggle('separator-top', !atTop);
      contentElement.classList.toggle('fade-bottom', !atBottom);
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'file-handler-page': FileHandlerPageElement;
  }
}

customElements.define('file-handler-page', FileHandlerPageElement);
