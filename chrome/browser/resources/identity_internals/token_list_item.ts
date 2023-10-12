// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {sendWithPromise} from 'chrome://resources/js/cr.js';
import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {getTemplate} from './token_list_item.html.js';

declare global {
  interface HTMLElementEventMap {
    'remove-token': CustomEvent<string>;
  }
}

function dashToCamelCase(dashId: string): string {
  const words = dashId.split('-');
  let output = words[0];
  assert(output);
  words.slice(1).forEach(word => {
    output += word.charAt(0).toUpperCase() + word.slice(1);
  });
  return output;
}

class TokenListItemElement extends CustomElement {
  static override get template() {
    return getTemplate();
  }

  extensionId: string = '';

  configure(data: {[key: string]: string|string[]}) {
    this.id = data['accessToken'] as string;
    this.extensionId = data['extensionId'] as string;

    ['access-token', 'extension-name', 'extension-id', 'account-id', 'status',
     'expiration-time']
        .forEach(identifier => {
          const element =
              this.shadowRoot!.querySelector<HTMLElement>(`.${identifier}`);
          if (element) {
            element.textContent = data[dashToCamelCase(identifier)] as string;
          }
        });

    const scopeList =
        this.shadowRoot!.querySelector<HTMLElement>('.scope-list');
    assert(scopeList);
    (data['scopes'] as string[]).forEach(scope => {
      scopeList.appendChild(document.createTextNode(scope));
      scopeList.appendChild(document.createElement('br'));
    });

    const revokeButton =
        this.shadowRoot!.querySelector<HTMLElement>('.revoke-button');
    assert(revokeButton);
    revokeButton.addEventListener('click', () => {
      sendWithPromise('identityInternalsRevokeToken', this.extensionId, this.id)
          .then(token => {
            this.dispatchEvent(new CustomEvent(
                'remove-token',
                {bubbles: true, composed: true, detail: token}));
          });
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'token-list-item': TokenListItemElement;
  }
}

customElements.define('token-list-item', TokenListItemElement);
