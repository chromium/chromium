// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_auto_img/cr_auto_img.js';
import 'chrome://resources/cros_components/button/button.js';
import './strings.m.js';

import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {Button} from 'chrome://resources/cros_components/button/button.js';
import {assert, assertInstanceof, assertNotReached} from 'chrome://resources/js/assert.js';
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

  constructor() {
    super();
    const template = document.createElement('template');
    template.innerHTML = AppInstallDialogElement.template as string;
    const fragment = template.content.cloneNode(true);
    this.attachShadow({mode: 'open'}).appendChild(fragment);

    this.initDynamicContent();
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
      urlElement.textContent = dialogArgs.args.url.url;

      const iconElement = this.$<HTMLImageElement>('#app-icon');
      assert(iconElement);
      iconElement.setAttribute('auto-src', dialogArgs.args.iconUrl.url);

      if (dialogArgs.args.description) {
        this.$<HTMLDivElement>('#description').textContent =
            dialogArgs.args.description;
        this.$<HTMLDivElement>('#description-and-screenshots').hidden = false;
        this.$<HTMLHRElement>('#divider').hidden = false;
      }

      if (dialogArgs.args.screenshotUrls[0]) {
        this.$<HTMLSpanElement>('#description-and-screenshots').hidden = false;
        this.$<HTMLHRElement>('#divider').hidden = false;
        this.$<HTMLSpanElement>('#screenshot-container').hidden = false;
        this.$<HTMLImageElement>('#screenshot')
            .setAttribute('auto-src', dialogArgs.args.screenshotUrls[0].url);
      }
    } catch (e) {
      // TODO(crbug.com/1488697) Define expected behavior.
      console.error(`Unable to get dialog arguments . Error: ${e}.`);
    }
  }

  $<T extends Element>(query: string): T {
    return this.shadowRoot!.querySelector(query)!;
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
    const installButton = this.$<Button>('.install-button')!;
    assert(installButton);
    switch (state) {
      case DialogState.INSTALL:
        this.$<HTMLElement>('#title-icon-install').style.display = 'block';
        this.$<HTMLElement>('#title-icon-installed').style.display = 'none';
        this.$<HTMLElement>('#title').textContent =
            loadTimeData.getString('installAppToDevice');

        installButton.disabled = false;
        installButton.label = loadTimeData.getString('install');
        installButton.addEventListener(
            'click', this.onInstallButtonClick.bind(this), {once: true});

        this.$<HTMLElement>('#installing-icon').setAttribute('slot', '');
        this.$<HTMLElement>('#install-icon')
            .setAttribute('slot', 'leading-icon');
        break;
      case DialogState.INSTALLING:
        this.$<HTMLElement>('#title').textContent =
            loadTimeData.getString('installingApp');

        installButton.disabled = true;
        installButton.label = loadTimeData.getString('installing');
        installButton.classList.replace('install', 'installing');

        this.$<HTMLElement>('#install-icon').setAttribute('slot', '');
        this.$<HTMLElement>('#installing-icon')
            .setAttribute('slot', 'leading-icon');
        break;
      case DialogState.INSTALLED:
        this.$<HTMLElement>('#title-icon-install').style.display = 'none';
        this.$<HTMLElement>('#title-icon-installed').style.display = 'block';
        this.$<HTMLElement>('#title').textContent =
            loadTimeData.getString('appInstalled');

        installButton.disabled = false;
        installButton.label = loadTimeData.getString('openApp');
        installButton.classList.replace('installing', 'installed');
        installButton.addEventListener(
            'click', this.onOpenAppButtonClick.bind(this));

        this.$<HTMLElement>('#installing-icon').setAttribute('slot', '');
        this.$<HTMLElement>('#installed-icon')
            .setAttribute('slot', 'leading-icon');
        break;
      default:
        assertNotReached();
    }
  }
}

customElements.define(AppInstallDialogElement.is, AppInstallDialogElement);
