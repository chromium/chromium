// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/paper-ripple/paper-ripple.js';

import {getTemplate} from './file_handler_card.html.js';

/**
 * The BaseCardElement defines the base class for all the file handler and
 * accordion cards.
 */
export class BaseCardElement extends HTMLElement {
  constructor() {
    super();
    const shadowRoot = this.attachShadow({mode: 'open'});
    shadowRoot.innerHTML = getTemplate();
    this.addStyles();
    this.addEventListener('keyup', this.onKeyUp.bind(this));
  }

  $(query: string): HTMLElement {
    return this.shadowRoot!.querySelector(query)!;
  }

  addStyles() {
    this.$('#container')!.classList.add(
        'margin-top', 'round-top', 'round-bottom');
    this.tabIndex = 0;
  }

  onKeyUp(e: KeyboardEvent) {
    if (e.key !== ' ' && e.key !== 'Enter') {
      return;
    }

    e.preventDefault();
    e.stopPropagation();

    this.click();
  }
}

/**
 * The AccordionTopCardElement defines the card at the top of the
 * "Other apps" accordion.
 */
export class AccordionTopCardElement extends BaseCardElement {
  private expanded_ = false;

  override addStyles() {
    super.addStyles();
    this.$('#icon')!.style.display = 'none';
    this.$('#title')!.textContent = 'Other apps';
    this.$('#right-icon')!.classList.add('chevron');
    this.ariaExpanded = 'false';
    this.role = 'button';
  }

  toggleExpandedState(): boolean {
    this.expanded_ = !this.expanded_;
    if (this.expanded_) {
      this.$('#container')!.classList.add('separator-bottom');
      this.$('#container')!.classList.remove('round-bottom');
      this.$('#right-icon')!.setAttribute('expanded', '');
      this.ariaExpanded = 'true';
    } else {
      this.$('#container')!.classList.remove('separator-bottom');
      this.$('#container')!.classList.add('round-bottom');
      this.$('#right-icon')!.removeAttribute('expanded');
      this.ariaExpanded = 'false';
    }
    return this.expanded_;
  }

  get expanded(): boolean {
    return this.expanded_;
  }
}

/**
 * The FileHandlerCardElement defines the base class for the cloud provider and
 * local handler cards.
 */
export class FileHandlerCardElement extends BaseCardElement {
  private selected_ = false;

  constructor() {
    super();
    this.ariaSelected = 'false';
    this.ariaCurrent = 'false';
    this.role = 'option';
  }

  updateSelection(selected: boolean) {
    this.selected_ = selected;
    if (this.selected_) {
      this.$('#card')!.setAttribute('selected', '');
      this.ariaSelected = 'true';
      this.ariaCurrent = 'true';
    } else {
      this.$('#card')!.removeAttribute('selected');
      this.ariaSelected = 'false';
      this.ariaCurrent = 'false';
    }
  }

  get selected(): boolean {
    return this.selected_;
  }
}

export enum CloudProviderType {
  NONE,
  DRIVE,
  ONE_DRIVE,
}

export class CloudProviderCardElement extends FileHandlerCardElement {
  private type_ = CloudProviderType.NONE;

  setParameters(type: CloudProviderType, name: string, description: string) {
    this.type_ = type;
    this.$('#title')!.textContent = name;
    this.$('#description')!.textContent = description;
  }

  setIconClass(className: string) {
    this.$('#icon')!.classList.add(className);
  }

  get type(): CloudProviderType {
    return this.type_;
  }
}

export class LocalHandlerCardElement extends FileHandlerCardElement {
  private taskPosition_ = -1;

  override addStyles() {}

  setParameters(taskPosition: number, name: string) {
    this.taskPosition_ = taskPosition;
    this.$('#title')!.textContent = name;
  }

  setIconUrl(url: string) {
    this.$('#icon')!.setAttribute(
        'style', 'background-image: url(' + url + ')');
  }

  show() {
    this.style.display = '';
    this.tabIndex = 0;
  }

  hide() {
    this.style.display = 'none';
    this.tabIndex = -1;
  }

  get taskPosition(): number {
    return this.taskPosition_;
  }
}

customElements.define('accordion-top-card', AccordionTopCardElement);
customElements.define('cloud-provider-card', CloudProviderCardElement);
customElements.define('local-handler-card', LocalHandlerCardElement);
