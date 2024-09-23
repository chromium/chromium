// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';
import '//resources/ash/common/cr_elements/cr_button/cr_button.js';

import {ColorChangeUpdater} from '//resources/cr_components/color_change_listener/colors_css_updater.js';
import {assert} from '//resources/js/assert.js';

import {DialogChoice} from './office_fallback.mojom-webui.js';
import {OfficeFallbackBrowserProxy} from './office_fallback_browser_proxy.js';
import {getTemplate} from './office_fallback_dialog.html.js';

window.addEventListener('load', () => {
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
  enableRetryOption = true;
  enableQuickOfficeOption = true;
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
    const tryAgainButton = this.$('#try-again-button')!;
    const cancelButton = this.$('#cancel-button')!;
    const okButton = this.$('#ok-button')!;
    const quickOfficeButton = this.$('#quick-office-button')!;
    tryAgainButton.addEventListener(
        'click', () => this.onTryAgainButtonClick());
    cancelButton.addEventListener('click', () => this.onCancelButtonClick());
    okButton.addEventListener('click', () => this.onOkButtonClick());
    quickOfficeButton.addEventListener(
        'click', () => this.onQuickOfficeButtonClick());
    document.addEventListener('keydown', this.onKeyDown.bind(this));
    if (this.enableRetryOption) {
      this.$('#ok-button')!.style.display = 'none';
    } else {
      this.$('#try-again-button')!.style.display = 'none';
      this.$('#cancel-button')!.style.display = 'none';
    }
    if (!this.enableQuickOfficeOption) {
      this.$('#quick-office-button')!.style.display = 'none';
    }
    if (this.reasonMessage === '') {
      this.$('#reason-message')!.style.display = 'none';
    }
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
      assert(args.instructionsMessage);
      this.titleText = args.titleText;
      this.reasonMessage = args.reasonMessage;
      this.instructionsMessage = args.instructionsMessage;
      this.enableRetryOption = args.enableRetryOption;
      this.enableQuickOfficeOption = args.enableQuickOfficeOption;
    } catch (e) {
      console.error(`Unable to get dialog arguments. Error: ${e}.`);
    }
  }

  createTemplate(): HTMLTemplateElement {
    const template = document.createElement('template');
    template.innerHTML = getTemplate() as string;
    const fragment = template.content;
    const titleElement = fragment.querySelector<HTMLElement>('#title');
    assert(titleElement);
    const reasonMessageElement =
        fragment.querySelector<HTMLElement>('#reason-message');
    assert(reasonMessageElement);
    const instructionsMessageElement =
        fragment.querySelector<HTMLElement>('#instructions-message');
    assert(instructionsMessageElement);

    titleElement.textContent = this.titleText;
    reasonMessageElement.textContent = this.reasonMessage;
    instructionsMessageElement.textContent = this.instructionsMessage;
    return template;
  }

  private onTryAgainButtonClick(): void {
    this.proxy.handler.close(DialogChoice.kTryAgain);
  }

  private onCancelButtonClick(): void {
    this.proxy.handler.close(DialogChoice.kCancel);
  }

  private onOkButtonClick(): void {
    this.proxy.handler.close(DialogChoice.kOk);
  }

  private onQuickOfficeButtonClick(): void {
    this.proxy.handler.close(DialogChoice.kQuickOffice);
  }

  private onKeyDown(e: KeyboardEvent) {
    if (e.key === 'Escape') {
      // Handle Escape as a "cancel" (which can therefore still be returned as a
      // response when the "cancel" button is hidden).
      e.stopImmediatePropagation();
      e.preventDefault();
      this.onCancelButtonClick();
      return;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'office-fallback': OfficeFallbackElement;
  }
}

customElements.define('office-fallback', OfficeFallbackElement);
