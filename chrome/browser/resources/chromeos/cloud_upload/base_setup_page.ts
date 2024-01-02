// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'base-setup-page' defines the base code and UI for all pages of the setup
 * flow. It also defines the events that each page fires to inform the dialog
 * to switch pages or exit.
 */

import {getTemplate} from './base_setup_page.html.js';

/**
 * The event fired to inform the dialog to switch to the next page.
 */
export const NEXT_PAGE_EVENT = 'next-page';

/**
 * The event fired to inform the dialog to cancel setup and close the dialog.
 */
export const CANCEL_SETUP_EVENT = 'cancel-setup';

declare global {
  interface HTMLElementEventMap {
    [NEXT_PAGE_EVENT]: CustomEvent<void>;
    [CANCEL_SETUP_EVENT]: CustomEvent<void>;
  }
}

/**
 * The base class for all setup page elements. This defines the basic page
 * layout via a common shadow DOM.
 */
export class BaseSetupPageElement extends HTMLElement {
  constructor() {
    super();

    const template = document.createElement('template');
    template.innerHTML = getTemplate();
    this.attachShadow({mode: 'open'})
        .appendChild(template.content.cloneNode(true));
  }

  /**
   * Initialises the page specific content inside the page.
   */
  connectedCallback(): void {
    const contentElement =
        this.shadowRoot!.querySelector<HTMLElement>('#content')!;
    this.updateContentFade(contentElement);
    contentElement.addEventListener(
        'scroll', this.updateContentFade.bind(undefined, contentElement),
        {passive: true});
    // Focus the dialog so that the screen reader reads the title.
    this.shadowRoot!.querySelector<HTMLElement>('#dialog')!.focus();
  }

  updateContentFade(contentElement: HTMLElement) {
    window.requestAnimationFrame(() => {
      const atTop = contentElement.scrollTop === 0;
      const atBottom =
          contentElement.scrollHeight - contentElement.scrollTop ===
          contentElement.clientHeight;
      contentElement.classList.toggle('fade-top', !atTop);
      contentElement.classList.toggle('fade-bottom', !atBottom);
    });
  }
}
