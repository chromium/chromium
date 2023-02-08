// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getTemplate} from './file_handler_card.html.js';

/**
 * The BaseCardElement defines the base class for all the file handler and
 * accordion cards.
 */
class BaseCardElement extends HTMLElement {
  constructor() {
    super();
    const shadowRoot = this.attachShadow({mode: 'open'});
    shadowRoot.innerHTML = getTemplate();
    this.addStyles();
  }

  $(query: string): HTMLElement {
    return this.shadowRoot!.querySelector(query)!;
  }

  addStyles() {
    this.$('#card')!.classList.add('margin-top', 'round-top', 'round-bottom');
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
    this.$('#title')!.innerHTML = 'Other apps';
    this.$('#right-icon')!.classList.add('chevron');
  }

  toggleExpandedState(): boolean {
    this.expanded_ = !this.expanded_;
    if (this.expanded_) {
      this.$('#card')!.classList.add('separator-bottom');
      this.$('#card')!.classList.remove('round-bottom');
      this.$('#right-icon')!.setAttribute('expanded', '');
    } else {
      this.$('#card')!.classList.remove('separator-bottom');
      this.$('#card')!.classList.add('round-bottom');
      this.$('#right-icon')!.removeAttribute('expanded');
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

  updateSelection(selected: boolean) {
    this.selected_ = selected;
    if (this.selected_) {
      this.$('#card')!.setAttribute('checked', '');
    } else {
      this.$('#card')!.removeAttribute('checked');
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
    this.$('#title')!.innerHTML = name;
    this.$('#description')!.innerHTML = description;
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
    this.$('#title')!.innerHTML = name;
  }

  setIconUrl(url: string) {
    this.$('#icon')!.setAttribute(
        'style', 'background-image: url(' + url + ')');
  }

  show() {
    this.style.display = '';
  }

  hide() {
    this.style.display = 'none';
  }

  get taskPosition(): number {
    return this.taskPosition_;
  }
}

customElements.define('accordion-top-card', AccordionTopCardElement);
customElements.define('cloud-provider-card', CloudProviderCardElement);
customElements.define('local-handler-card', LocalHandlerCardElement);
