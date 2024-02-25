// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './cr_a11y_announcer.html.js';

/**
 * The CrA11yAnnouncerElement is a visually hidden element that reads out
 * messages to a screen reader. This is preferred over IronA11yAnnouncer.
 *
 * Forked from
 * ui/webui/resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.ts
 * @fileoverview
 */

type CrA11yAnnouncerMessagesSentEvent = CustomEvent<{
  messages: string[],
}>;

declare global {
  interface HTMLElementEventMap {
    'cr-a11y-announcer-messages-sent': CrA11yAnnouncerMessagesSentEvent;
  }
}

/**
 * 150ms seems to be around the minimum time required for screen readers to
 * read out consecutively queued messages.
 */
export const TIMEOUT_MS: number = 150;

/**
 * A map of an HTML element to its corresponding CrA11yAnnouncerElement. There
 * may be multiple CrA11yAnnouncerElements on a page, especially for cases in
 * which the DocumentElement's CrA11yAnnouncerElement becomes hidden or
 * deactivated (eg. when a modal dialog causes the CrA11yAnnouncerElement to
 * become inaccessible).
 */
const instances: Map<HTMLElement, CrA11yAnnouncerElement> = new Map();

export function getInstance(container: HTMLElement = document.body):
    CrA11yAnnouncerElement {
  if (instances.has(container)) {
    return instances.get(container)!;
  }
  assert(container.isConnected);
  const instance = new CrA11yAnnouncerElement();
  container.appendChild(instance);
  instances.set(container, instance);
  return instance;
}

export class CrA11yAnnouncerElement extends PolymerElement {
  static get is() {
    return 'cr-a11y-announcer';
  }

  static get template() {
    return getTemplate();
  }

  private currentTimeout_: number|null = null;
  private messages_: string[] = [];

  override disconnectedCallback() {
    super.disconnectedCallback();
    if (this.currentTimeout_ !== null) {
      clearTimeout(this.currentTimeout_);
      this.currentTimeout_ = null;
    }

    for (const [parent, instance] of instances) {
      if (instance === this) {
        instances.delete(parent);
        break;
      }
    }
  }

  announce(message: string) {
    if (this.currentTimeout_ !== null) {
      clearTimeout(this.currentTimeout_);
      this.currentTimeout_ = null;
    }

    this.messages_.push(message);

    this.currentTimeout_ = setTimeout(() => {
      const messagesDiv = this.shadowRoot!.querySelector('#messages')!;
      messagesDiv.innerHTML = window.trustedTypes!.emptyHTML;

      for (const message of this.messages_) {
        const div = document.createElement('div');
        div.textContent = message;
        messagesDiv.appendChild(div);
      }

      // Dispatch a custom event to allow consumers to know when certain alerts
      // have been sent to the screen reader.
      this.dispatchEvent(new CustomEvent(
          'cr-a11y-announcer-messages-sent',
          {bubbles: true, detail: {messages: this.messages_.slice()}}));

      this.messages_.length = 0;
      this.currentTimeout_ = null;
    }, TIMEOUT_MS);
  }
}

customElements.define(CrA11yAnnouncerElement.is, CrA11yAnnouncerElement);
