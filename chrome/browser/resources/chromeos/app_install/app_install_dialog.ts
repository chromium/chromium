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
}

interface StateData {
  title: {
    iconIdQuery: string,
    labelId: string,
  };
  actionButton: {
    disabled?: boolean, labelId: string, handler: (event: MouseEvent) => void,
    handleOnce?: boolean, iconIdQuery: string,
  };
  cancelButtonLabelId: string;
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
  private dialogStateDataMap: Record<DialogState, StateData>;

  constructor() {
    super();
    const template = document.createElement('template');
    template.innerHTML = AppInstallDialogElement.template as string;
    const fragment = template.content.cloneNode(true);
    this.attachShadow({mode: 'open'}).appendChild(fragment);

    this.initDynamicContent();

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
          iconIdQuery: '#install-icon',
        },
        cancelButtonLabelId: 'cancel',
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
          iconIdQuery: '#installing-icon',
        },
        cancelButtonLabelId: 'cancel',
      },
      [DialogState.INSTALLED]: {
        title: {
          iconIdQuery: '#title-icon-installed',
          labelId: 'appInstalled',
        },
        actionButton: {
          labelId: 'openApp',
          handler: () => this.onOpenAppButtonClick(),
          iconIdQuery: '#installed-icon',
        },
        cancelButtonLabelId: 'close',
      },
    };
  }

  async initDynamicContent() {
    try {
      const dialogArgs = await this.proxy.handler.getDialogArgs();
      assert(dialogArgs.args);

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
    } catch (e) {
      // TODO(crbug.com/1488697) Define expected behavior.
      console.error(`Unable to get dialog arguments . Error: ${e}.`);
    }
  }

  $<T extends Element>(query: string): T {
    return this.shadowRoot!.querySelector(query)!;
  }

  $$(query: string): HTMLElement[] {
    return Array.from(this.shadowRoot!.querySelectorAll(query));
  }

  connectedCallback(): void {
    const cancelButton = this.$<Button>('.cancel-button');
    assert(cancelButton);
    cancelButton.addEventListener('click', this.onCancelButtonClick.bind(this));

    this.changeDialogState(DialogState.INSTALL);
  }

  private onCancelButtonClick(): void {
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

    if (install_result) {
      this.changeDialogState(DialogState.INSTALLED);
    } else {
      // TODO(crbug.com/1488697): Proper error display.
      this.changeDialogState(DialogState.INSTALL);
    }
  }

  private async onOpenAppButtonClick() {
    this.proxy.handler.launchApp();
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
    cancelButton.label = loadTimeData.getString(data.cancelButtonLabelId);
  }
}

customElements.define(AppInstallDialogElement.is, AppInstallDialogElement);
