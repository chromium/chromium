// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import './strings.m.js';

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {assert} from 'chrome://resources/js/assert.js';

import {DialogChoice} from './office_fallback.mojom-webui.js';
import {OfficeFallbackBrowserProxy} from './office_fallback_browser_proxy.js';
import {getTemplate} from './office_fallback_dialog.html.js';

window.addEventListener('load', () => {
  const jellyEnabled = loadTimeData.getBoolean('isJellyEnabled');
  const theme = jellyEnabled ? 'refresh23' : 'legacy';
  document.documentElement.setAttribute('theme', theme);
  ColorChangeUpdater.forDocument().start();
});

/**
 * The OfficeFallbackElement represents the dialog that allows the user to
 * choose what to do when failing to open office files.
 * @extends HTMLElement
 */
export class OfficeFallbackElement extends HTMLElement {
  titleText: string = '';
  reasonMessage: string = '';
  instructionsMessage: string = '';
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

  /**
   * Initialises the class members based off the given dialog arguments.
   */
  processDialogArgs() {
    try {
      const dialogArgs = this.proxy.getDialogArguments();
      assert(dialogArgs);
      const args = JSON.parse(dialogArgs);
      assert(args);
      assert(args.titleText);
      assert(args.reasonMessage);
      assert(args.instructionsMessage);
      this.titleText = args.titleText;
      this.reasonMessage = args.reasonMessage;
      this.instructionsMessage = args.instructionsMessage;
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
    const instructionsMessageElement =
        fragment.querySelector('#instructions-message')! as HTMLElement;

    titleElement.textContent = this.titleText;
    reasonMessageElement.textContent = this.reasonMessage;
    instructionsMessageElement.textContent = this.instructionsMessage;
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

declare global {
  interface HTMLElementTagNameMap {
    'office-fallback': OfficeFallbackElement;
  }
}

customElements.define('office-fallback', OfficeFallbackElement);
