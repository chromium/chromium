// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_auto_img/cr_auto_img.js';
import 'chrome://resources/cros_components/button/button.js';
import './strings.m.js';

import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {Button} from 'chrome://resources/cros_components/button/button.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import type {DialogArgs} from './app_install.mojom-webui.js';
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
  FAILED_INSTALL_ERROR = 'failed_install_error',
  NO_APP_ERROR = 'no_app_error',
  CONNECTION_ERROR = 'connection_error',
}

interface StateData {
  title: {
    iconIdQuery: string,
    labelId: string,
  };
  content?: {
    hidden?: boolean,
  };
  errorMessage?: {
    visible?: boolean, textId: string,
  };
  actionButton: {
    hidden?: boolean,
    disabled?: boolean,
    labelId?: string,
    handler?: () => void,
    handleOnce?: boolean,
    iconIdQuery?: string,
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
  private dialogArgs?: DialogArgs;

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
          handler: () => this.onInstallButtonClick(),
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
      [DialogState.FAILED_INSTALL_ERROR]: {
        title: {
          iconIdQuery: '#title-icon-error',
          labelId: 'failedInstall',
        },
        actionButton: {
          labelId: 'tryAgain',
          handler: () => this.onInstallButtonClick(),
          handleOnce: true,
          iconIdQuery: '#action-icon-try-again',
        },
        cancelButton: {
          labelId: 'cancel',
        },
      },
      [DialogState.NO_APP_ERROR]: {
        title: {
          iconIdQuery: '#title-icon-error',
          labelId: 'noAppErrorTitle',
        },
        content: {
          hidden: true,
        },
        errorMessage: {
          visible: true,
          textId: 'noAppErrorDescription',
        },
        actionButton: {
          hidden: true,
        },
        cancelButton: {
          labelId: 'close',
        },
      },
      [DialogState.CONNECTION_ERROR]: {
        title: {
          iconIdQuery: '#title-icon-connection-error',
          labelId: 'connectionErrorTitle',
        },
        content: {
          hidden: true,
        },
        errorMessage: {
          visible: true,
          textId: 'connectionErrorDescription',
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
    };
  }

  async initContent() {
    const cancelButton = this.$<Button>('.cancel-button');
    assert(cancelButton);
    cancelButton.addEventListener('click', this.onCancelButtonClick.bind(this));

    try {
      this.dialogArgs = (await this.proxy.handler.getDialogArgs()).dialogArgs;

      if (this.dialogArgs.noAppErrorArgs) {
        return DialogState.NO_APP_ERROR;
      }

      if (this.dialogArgs.connectionErrorActions) {
        return DialogState.CONNECTION_ERROR;
      }

      const appInfo = this.dialogArgs.appInfoArgs!.data;

      const nameElement = this.$<HTMLParagraphElement>('#name');
      assert(nameElement);
      nameElement.textContent = appInfo.name;

      const urlElement = this.$<HTMLAnchorElement>('#url-link');
      assert(urlElement);
      urlElement.textContent = new URL(appInfo.url.url).hostname;
      urlElement.setAttribute('href', new URL(appInfo.url.url).origin);

      const iconElement = this.$<HTMLImageElement>('#app-icon');
      assert(iconElement);
      iconElement.setAttribute('auto-src', appInfo.iconUrl.url);
      iconElement.setAttribute(
          'alt',
          loadTimeData.substituteString(
              loadTimeData.getString('iconAlt'), appInfo.name));

      if (appInfo.description) {
        this.$<HTMLDivElement>('#description').textContent =
            appInfo.description;
        this.$<HTMLDivElement>('#description-and-screenshots').hidden = false;
        this.$<HTMLHRElement>('#divider').hidden = false;
      }

      if (appInfo.screenshots[0]) {
        this.$<HTMLSpanElement>('#description-and-screenshots').hidden = false;
        this.$<HTMLHRElement>('#divider').hidden = false;
        this.$<HTMLDivElement>('#screenshot-container').hidden = false;
        const height = appInfo.screenshots[0].size.height /
            (appInfo.screenshots[0].size.width / 408);
        this.$<HTMLDivElement>('#screenshot-container').style.height =
            height.toString() + 'px';
        this.$<HTMLImageElement>('#screenshot').onload = () => {
          this.onScreenshotLoad();
        };
        this.$<HTMLImageElement>('#screenshot')
            .setAttribute('auto-src', appInfo.screenshots[0].url.url);
      }

      return appInfo.isAlreadyInstalled ? DialogState.ALREADY_INSTALLED :
                                          DialogState.INSTALL;
    } catch (e) {
      console.error(`Unable to get dialog arguments . Error: ${e}.`);
      return DialogState.NO_APP_ERROR;
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

  private onScreenshotLoad(): void {
    this.$<HTMLImageElement>('#screenshot')!.style.display = 'block';
  }

  private onCancelButtonClick(): void {
    if (this.$<Button>('.cancel-button').disabled) {
      return;
    }
    this.proxy.handler.closeDialog();
  }

  private async onInstallButtonClick() {
    this.changeDialogState(DialogState.INSTALLING);

    // Keep the installing state shown for at least 2 seconds to give the
    // impression that the PWA is being installed.
    const [{installed: install_result}] = await Promise.all([
      this.dialogArgs!.appInfoArgs!.actions.installApp(),
      new Promise(resolve => setTimeout(resolve, 2000)),
    ]);

    this.changeDialogState(
        install_result ? DialogState.INSTALLED :
                         DialogState.FAILED_INSTALL_ERROR);
  }

  private async onOpenAppButtonClick() {
    this.dialogArgs!.appInfoArgs!.actions.launchApp();
    this.proxy.handler.closeDialog();
  }

  private async onTryAgainButtonClick() {
    this.dialogArgs!.connectionErrorActions!.tryAgain();
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
    contentCard.style.display = data.content?.hidden ? 'none' : 'block';

    const errorMessage = this.$<HTMLElement>('#error-message')!;
    errorMessage.style.display = data.errorMessage?.visible ? 'block' : 'none';
    if (data.errorMessage) {
      errorMessage.textContent =
          loadTimeData.getString(data.errorMessage.textId);
    }

    const actionButton = this.$<Button>('.action-button')!;
    assert(actionButton);
    actionButton.style.display = data.actionButton.hidden ? 'none' : 'block';
    actionButton.disabled = Boolean(data.actionButton.disabled);
    if (data.actionButton.labelId) {
      actionButton.label = loadTimeData.getString(data.actionButton.labelId);
    }
    if (data.actionButton.handler) {
      actionButton.addEventListener(
          'click', data.actionButton.handler,
          {once: Boolean(data.actionButton.handleOnce)});
    }
    for (const icon of this.$$('.action-icon')) {
      icon.setAttribute('slot', '');
    }
    if (data.actionButton.iconIdQuery) {
      this.$<HTMLElement>(data.actionButton.iconIdQuery)
          .setAttribute('slot', 'leading-icon');
    }

    const cancelButton = this.$<Button>('.cancel-button');
    assert(cancelButton);
    cancelButton.disabled = Boolean(data.cancelButton.disabled);
    cancelButton.label = loadTimeData.getString(data.cancelButton.labelId);
  }
}

customElements.define(AppInstallDialogElement.is, AppInstallDialogElement);
