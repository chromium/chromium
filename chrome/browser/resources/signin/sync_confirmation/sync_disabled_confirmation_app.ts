// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import './strings.m.js';

import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SyncConfirmationBrowserProxy} from './sync_confirmation_browser_proxy.js';
import {ScreenMode, SyncConfirmationBrowserProxyImpl} from './sync_confirmation_browser_proxy.js';
import {getCss} from './sync_disabled_confirmation_app.css.js';
import {getHtml} from './sync_disabled_confirmation_app.html.js';

export interface SyncDisabledConfirmationAppElement {
  $: {
    confirmButton: HTMLElement,
  };
}

export class SyncDisabledConfirmationAppElement extends CrLitElement {
  static get is() {
    return 'sync-disabled-confirmation-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      signoutDisallowed_: {type: Boolean},
    };
  }

  protected signoutDisallowed_: boolean =
      loadTimeData.getBoolean('signoutDisallowed');
  private syncConfirmationBrowserProxy_: SyncConfirmationBrowserProxy =
      SyncConfirmationBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    document.addEventListener(
        'keydown', e => this.onKeyDown_(/** @type {!KeyboardEvent} */ (e)));
  }

  protected onConfirm_(e: Event) {
    this.syncConfirmationBrowserProxy_.confirm(
        this.getConsentDescription_(),
        this.getConsentConfirmation_(e.composedPath() as HTMLElement[]),
        ScreenMode.UNSUPPORTED);
  }

  /**
   * @param path Path of the click event. Must contain a consent confirmation
   *     element.
   * @return The text of the consent confirmation element.
   */
  private getConsentConfirmation_(path: HTMLElement[]): string {
    for (const element of path) {
      if (element.nodeType !== Node.DOCUMENT_FRAGMENT_NODE &&
          element.hasAttribute('consent-confirmation')) {
        return element.innerHTML.trim();
      }
    }
    assertNotReached('No consent confirmation element found.');
  }

  /** @return Text of the consent description elements. */
  private getConsentDescription_(): string[] {
    const consentDescription =
        Array.from(this.shadowRoot!.querySelectorAll('[consent-description]'))
            .filter(element => element.clientWidth * element.clientHeight > 0)
            .map(element => element.innerHTML.trim());
    assert(consentDescription);
    return consentDescription;
  }

  private onKeyDown_(e: KeyboardEvent) {
    // If the currently focused element isn't something that performs an action
    // on "enter" being pressed and the user hits "enter", perform the default
    // action of the dialog, which is "OK, Got It".
    if (e.key === 'Enter' &&
        !/^(A|PAPER-(BUTTON|CHECKBOX))$/.test(
            document.activeElement!.tagName)) {
      this.$.confirmButton.click();
      e.preventDefault();
    }
  }

  protected onUndo_() {
    this.syncConfirmationBrowserProxy_.undo(ScreenMode.UNSUPPORTED);
  }
}

customElements.define(
    SyncDisabledConfirmationAppElement.is, SyncDisabledConfirmationAppElement);
