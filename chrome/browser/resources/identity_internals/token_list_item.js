// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.m.js';
import {CustomElement} from 'chrome://resources/js/custom_element.js';
import {getTrustedHTML} from 'chrome://resources/js/static_types.js';

function dashToCamelCase(dashId) {
  const words = dashId.split('-');
  let output = words[0];
  words.slice(1).forEach(word => {
    output += word.charAt(0).toUpperCase() + word.slice(1);
  });
  return output;
}

class TokenListItemElement extends CustomElement {
  static get template() {
    return getTrustedHTML`{__html_template__}`;
  }

  constructor() {
    super();
    this.extensionId = '';
  }

  configure(data) {
    this.id = data.accessToken;
    this.extensionId = data.extensionId;

    ['access-token', 'extension-name', 'extension-id', 'account-id', 'status',
     'expiration-time']
        .forEach(identifier => {
          const element = this.shadowRoot.querySelector(`.${identifier}`);
          if (element) {
            element.textContent = data[dashToCamelCase(identifier)];
          }
        });

    const scopeList = this.shadowRoot.querySelector('.scope-list');
    data.scopes.forEach(scope => {
      scopeList.appendChild(document.createTextNode(scope));
      scopeList.appendChild(document.createElement('br'));
    });

    const revokeButton = this.shadowRoot.querySelector('.revoke-button');
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

customElements.define('token-list-item', TokenListItemElement);
