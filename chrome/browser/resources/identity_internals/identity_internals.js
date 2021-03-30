// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
import {sendWithPromise} from 'chrome://resources/js/cr.m.js';
import {define} from 'chrome://resources/js/cr/ui.m.js';
import {$} from 'chrome://resources/js/util.m.js';

let tokenList;

/**
 * Creates an identity token item.
 * @param {!Object} tokenInfo Object containing token information.
 * @constructor
 */
function TokenListItem(tokenInfo) {
  const el = document.createElement('div');
  el.data_ = tokenInfo;
  el.__proto__ = TokenListItem.prototype;
  el.decorate();
  return el;
}

TokenListItem.prototype = {
  __proto__: HTMLDivElement.prototype,

  /** @override */
  decorate() {
    this.textContent = '';
    this.id = this.data_.accessToken;

    const table = this.ownerDocument.createElement('table');
    const tbody = this.ownerDocument.createElement('tbody');
    tbody.appendChild(this.createEntry_(
        'Access Token', this.data_.accessToken, 'access-token'));
    tbody.appendChild(this.createEntry_(
        'Extension Name', this.data_.extensionName, 'extension-name'));
    tbody.appendChild(this.createEntry_(
        'Extension Id', this.data_.extensionId, 'extension-id'));
    tbody.appendChild(
        this.createEntry_('Account Id', this.data_.accountId, 'account-id'));
    tbody.appendChild(
        this.createEntry_('Token Status', this.data_.status, 'token-status'));
    tbody.appendChild(this.createEntry_(
        'Expiration Time', this.data_.expirationTime, 'expiration-time'));
    tbody.appendChild(this.createEntryForScopes_());
    table.appendChild(tbody);
    const tfoot = this.ownerDocument.createElement('tfoot');
    tfoot.appendChild(this.createButtons_());
    table.appendChild(tfoot);
    this.appendChild(table);
  },

  /**
   * Creates an entry for a single property of the token.
   * @param {string} label A label of the token's property name.
   * @param {string} value A value of the token property.
   * @param {string} accessor Additional class to tag the field for testing.
   * @return {HTMLElement} An HTML element with the property name and value.
   */
  createEntry_(label, value, accessor) {
    const row = this.ownerDocument.createElement('tr');
    const labelField = this.ownerDocument.createElement('td');
    labelField.classList.add('label');
    labelField.textContent = label;
    row.appendChild(labelField);
    const valueField = this.ownerDocument.createElement('td');
    valueField.classList.add('value');
    valueField.classList.add(accessor);
    valueField.textContent = value;
    row.appendChild(valueField);
    return row;
  },

  /**
   * Creates an entry for a list of token scopes.
   * @return {!HTMLElement} An HTML element with scopes.
   */
  createEntryForScopes_() {
    const row = this.ownerDocument.createElement('tr');
    const labelField = this.ownerDocument.createElement('td');
    labelField.classList.add('label');
    labelField.textContent = 'Scopes';
    row.appendChild(labelField);
    const valueField = this.ownerDocument.createElement('td');
    valueField.classList.add('value');
    valueField.classList.add('scope-list');
    this.data_.scopes.forEach(function(scope) {
      valueField.appendChild(this.ownerDocument.createTextNode(scope));
      valueField.appendChild(this.ownerDocument.createElement('br'));
    }, this);
    row.appendChild(valueField);
    return row;
  },

  /**
   * Creates buttons for the token.
   * @return {HTMLElement} An HTML element with actionable buttons for the
   *     token.
   */
  createButtons_() {
    const row = this.ownerDocument.createElement('tr');
    const buttonHolder = this.ownerDocument.createElement('td');
    buttonHolder.colSpan = 2;
    buttonHolder.classList.add('token-actions');
    buttonHolder.appendChild(this.createRevokeButton_());
    row.appendChild(buttonHolder);
    return row;
  },

  /**
   * Creates a revoke button with an event sending a revoke token message
   * to the controller.
   * @return {!HTMLButtonElement} The created revoke button.
   * @private
   */
  createRevokeButton_() {
    const revokeButton = this.ownerDocument.createElement('button');
    revokeButton.classList.add('revoke-button');
    revokeButton.addEventListener('click', function() {
      sendWithPromise(
          'identityInternalsRevokeToken', this.data_.extensionId,
          this.data_.accessToken)
          .then(token => tokenList.removeTokenNode_(token));
    }.bind(this));
    revokeButton.textContent = 'Revoke';
    return revokeButton;
  },
};

/**
 * Creates a new list of identity tokens.
 * @param {Object=} opt_propertyBag Optional properties.
 * @constructor
 */
const TokenList = define('div');

TokenList.prototype = {
  __proto__: HTMLDivElement.prototype,

  /** @override */
  decorate() {
    this.textContent = '';
    this.showTokenNodes_();
  },

  /**
   * Populates the list of tokens.
   */
  showTokenNodes_() {
    this.data_.forEach(function(tokenInfo) {
      this.appendChild(new TokenListItem(tokenInfo));
    }, this);
  },

  /**
   * Removes a token node related to the specified token ID from both the
   * internals data source as well as the user interface.
   * @param {string} accessToken The id of the token to remove.
   * @private
   */
  removeTokenNode_(accessToken) {
    let tokenIndex;
    for (let index = 0; index < this.data_.length; index++) {
      if (this.data_[index].accessToken == accessToken) {
        tokenIndex = index;
        break;
      }
    }

    // Remove from the data_ source if token found.
    if (tokenIndex) {
      this.data_.splice(tokenIndex, 1);
    }

    // Remove from the user interface.
    const tokenNode = $(accessToken);
    if (tokenNode) {
      this.removeChild(tokenNode);
    }

    this.dispatchEvent(
        new CustomEvent('token-removed-for-test', {detail: accessToken}));
  },
};

/**
 * Initializes the UI by asking the controller for list of identity tokens.
 */
document.addEventListener('DOMContentLoaded', () => {
  tokenList = $('token-list');
  tokenList.data_ = [];
  tokenList.__proto__ = TokenList.prototype;
  tokenList.decorate();
  sendWithPromise('identityInternalsGetTokens').then(tokens => {
    tokenList.data_ = tokens;
    tokenList.showTokenNodes_();
  });
});
