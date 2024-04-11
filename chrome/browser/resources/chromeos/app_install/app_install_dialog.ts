// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_auto_img/cr_auto_img.js';
import 'chrome://resources/cros_components/button/button.js';
import './strings.m.js';

import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {Button} from 'chrome://resources/cros_components/button/button.js';
import {assert, assertInstanceof} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {getTemplate} from './app_install_dialog.html.js';
import {BrowserProxy} from './browser_proxy.js';

window.addEventListener('load', () => {
  ColorChangeUpdater.forDocument().start();
});

enum DialogState {
  INSTALL = 'install',
  INSTALLING = 'installing',
  INSTALLED = 'installed',
  ALREADY_INSTALLED = 'already_installed',
  NO_DATA = 'no_data',
  FAILED_INSTALL = 'failed_install',
}

interface StateData {
  title: {
    iconIdQuery: string,
    labelId: string,
  };
  content?: {
    disabled?: boolean,
  };
  errorMessage?: {
    enabled?: boolean, textId: string,
  };
  actionButton: {
    disabled?: boolean, labelId: string, handler: (event: MouseEvent) => void,
    handleOnce?: boolean, iconIdQuery: string,
  };
  cancelButton: {
    disabled?: boolean, labelId: string,
  };
}

/**
 * @fileoverview
 * 'app-install-dialog' defines the UI for the ChromeOS app install dialog.
 */

class AppInstallDialogElement extends HTMLElement {
  static get is() {
    return 'app-install-dialog';
  }

  static get template() {
    return getTemplate();
  }

  private proxy = BrowserProxy.getInstance();
  private initialStatePromise: Promise<DialogState>;
  private dialogStateDataMap: Record<DialogState, StateData>;

  constructor() {
    super();
    const template = document.createElement('template');
    template.innerHTML = AppInstallDialogElement.template as string;
    const fragment = template.content.cloneNode(true);
    this.attachShadow({mode: 'open'}).appendChild(fragment);

    this.initialStatePromise = this.initContent();

    this.dialogStateDataMap = {
      [DialogState.INSTALL]: {
        title: {
          iconIdQuery: '#title-icon-install',
          labelId: 'installAppToDevice',
        },
        actionButton: {
          labelId: 'install',
          handler: (event: MouseEvent) => this.onInstallButtonClick(event),
          handleOnce: true,
          iconIdQuery: '#action-icon-install',
        },
        cancelButton: {
          labelId: 'cancel',
        },
      },
      [DialogState.INSTALLING]: {
        title: {
          iconIdQuery: '#title-icon-install',
          labelId: 'installingApp',
        },
        actionButton: {
          disabled: true,
          labelId: 'installing',
          handler() {},
          iconIdQuery: '#action-icon-installing',
        },
        cancelButton: {
          disabled: true,
          labelId: 'cancel',
        },
      },
      [DialogState.INSTALLED]: {
        title: {
          iconIdQuery: '#title-icon-installed',
          labelId: 'appInstalled',
        },
        actionButton: {
          labelId: 'openApp',
          handler: () => this.onOpenAppButtonClick(),
          iconIdQuery: '#action-icon-open-app',
        },
        cancelButton: {
          labelId: 'close',
        },
      },
      [DialogState.ALREADY_INSTALLED]: {
        title: {
          iconIdQuery: '#title-icon-installed',
          labelId: 'appAlreadyInstalled',
        },
        actionButton: {
          labelId: 'openApp',
          handler: () => this.onOpenAppButtonClick(),
          iconIdQuery: '#action-icon-open-app',
        },
        cancelButton: {
          labelId: 'close',
        },
      },
      [DialogState.NO_DATA]: {
        title: {
          iconIdQuery: '#title-icon-error',
          labelId: 'noAppDataTitle',
        },
        content: {
          disabled: true,
        },
        errorMessage: {
          enabled: true,
          textId: 'noAppDataDescription',
        },
        actionButton: {
          labelId: 'tryAgain',
          handler: () => this.onTryAgainButtonClick(),
          handleOnce: true,
          iconIdQuery: '#action-icon-try-again',
        },
        cancelButton: {
          labelId: 'cancel',
        },
      },
      [DialogState.FAILED_INSTALL]: {
        title: {
          iconIdQuery: '#title-icon-error',
          labelId: 'failedInstall',
        },
        actionButton: {
          labelId: 'tryAgain',
          handler: (event: MouseEvent) => this.onInstallButtonClick(event),
          handleOnce: true,
          iconIdQuery: '#action-icon-try-again',
        },
        cancelButton: {
          labelId: 'cancel',
        },
      },
    };
  }

  async initContent() {
    const cancelButton = this.$<Button>('.cancel-button');
    assert(cancelButton);
    cancelButton.addEventListener('click', this.onCancelButtonClick.bind(this));

    try {
      const dialogArgs = await this.proxy.handler.getDialogArgs();
      if (!dialogArgs.args) {
        return DialogState.NO_DATA;
      }

      const nameElement = this.$<HTMLParagraphElement>('#name');
      assert(nameElement);
      nameElement.textContent = dialogArgs.args.name;

      const urlElement = this.$<HTMLAnchorElement>('#url-link');
      assert(urlElement);
      urlElement.textContent = new URL(dialogArgs.args.url.url).hostname;
      urlElement.setAttribute('href', new URL(dialogArgs.args.url.url).origin);

      const iconElement = this.$<HTMLImageElement>('#app-icon');
      assert(iconElement);
      iconElement.setAttribute('auto-src', dialogArgs.args.iconUrl.url);
      iconElement.setAttribute(
          'alt',
          loadTimeData.substituteString(
              loadTimeData.getString('iconAlt'), dialogArgs.args.name));

      if (dialogArgs.args.description) {
        this.$<HTMLDivElement>('#description').textContent =
            dialogArgs.args.description;
        this.$<HTMLDivElement>('#description-and-screenshots').hidden = false;
        this.$<HTMLHRElement>('#divider').hidden = false;
      }

      if (dialogArgs.args.screenshots[0]) {
        this.$<HTMLSpanElement>('#description-and-screenshots').hidden = false;
        this.$<HTMLHRElement>('#divider').hidden = false;
        this.$<HTMLSpanElement>('#screenshot-container').hidden = false;
        this.$<HTMLImageElement>('#screenshot')
            .setAttribute('auto-src', dialogArgs.args.screenshots[0].url.url);
      }

      return dialogArgs.args.isAlreadyInstalled ?
          DialogState.ALREADY_INSTALLED :
          DialogState.INSTALL;
    } catch (e) {
      console.error(`Unable to get dialog arguments . Error: ${e}.`);
      return DialogState.NO_DATA;
    }
  }

  $<T extends Element>(query: string): T {
    return this.shadowRoot!.querySelector(query)!;
  }

  $$(query: string): HTMLElement[] {
    return Array.from(this.shadowRoot!.querySelectorAll(query));
  }

  async connectedCallback(): Promise<void> {
    this.changeDialogState(await this.initialStatePromise);
  }

  private onCancelButtonClick(): void {
    if (this.$<Button>('.cancel-button').disabled) {
      return;
    }
    this.proxy.handler.closeDialog();
  }

  private async onInstallButtonClick(event: MouseEvent) {
    assertInstanceof(event.target, Button);
    this.changeDialogState(DialogState.INSTALLING);

    // Keep the installing state shown for at least 2 seconds to give the
    // impression that the PWA is being installed.
    const [{installed: install_result}] = await Promise.all([
      this.proxy.handler.installApp(),
      new Promise(resolve => setTimeout(resolve, 2000)),
    ]);

    this.changeDialogState(
        install_result ? DialogState.INSTALLED : DialogState.FAILED_INSTALL);
  }

  private async onOpenAppButtonClick() {
    this.proxy.handler.launchApp();
    this.proxy.handler.closeDialog();
  }

  private async onTryAgainButtonClick() {
    this.proxy.handler.tryAgain();
    // TODO(b/333460441): Run the retry logic within the same dialog instead of
    // creating a new one.
    this.proxy.handler.closeDialog();
  }

  private changeDialogState(state: DialogState) {
    const data = this.dialogStateDataMap![state];
    assert(data);

    for (const icon of this.$$('.title-icon')) {
      icon.style.display = 'none';
    }
    this.$<HTMLElement>(data.title.iconIdQuery).style.display = 'block';
    this.$<HTMLElement>('#title').textContent =
        loadTimeData.getString(data.title.labelId);

    const contentCard = this.$<HTMLElement>('#content-card')!;
    contentCard.style.display = data.content?.disabled ? 'none' : 'block';

    const errorMessage = this.$<HTMLElement>('#error-message')!;
    errorMessage.style.display = data.errorMessage?.enabled ? 'block' : 'none';
    if (data.errorMessage) {
      errorMessage.textContent =
          loadTimeData.getString(data.errorMessage.textId);
    }

    const actionButton = this.$<Button>('.action-button')!;
    assert(actionButton);
    actionButton.disabled = Boolean(data.actionButton.disabled);
    actionButton.label = loadTimeData.getString(data.actionButton.labelId);
    actionButton.addEventListener(
        'click', data.actionButton.handler,
        {once: Boolean(data.actionButton.handleOnce)});

    for (const icon of this.$$('.action-icon')) {
      icon.setAttribute('slot', '');
    }
    this.$<HTMLElement>(data.actionButton.iconIdQuery)
        .setAttribute('slot', 'leading-icon');

    const cancelButton = this.$<Button>('.cancel-button');
    assert(cancelButton);
    cancelButton.disabled = Boolean(data.cancelButton.disabled);
    cancelButton.label = loadTimeData.getString(data.cancelButton.labelId);
  }
}

customElements.define(AppInstallDialogElement.is, AppInstallDialogElement);
