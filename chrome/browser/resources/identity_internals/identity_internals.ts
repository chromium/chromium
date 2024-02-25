// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './token_list_item.js';

import {assert} from 'chrome://resources/js/assert.js';
import {sendWithPromise} from 'chrome://resources/js/cr.js';

let data: Array<{[key: string]: string | string[]}> = [];
let list: HTMLElement;

/**
 * Removes a token node related to the specified token ID from both the
 * internals data source as well as the user interface.
 * @param e Contains the id of the token to remove.
 */
function removeTokenNode(e: CustomEvent<string>) {
  const accessToken = e.detail;
  let tokenIndex: number = -1;
  for (let index = 0; index < data.length; index++) {
    if (data[index]!['accessToken'] === accessToken) {
      tokenIndex = index;
      break;
    }
  }

  // Remove from the tokens_ source if token found.
  if (tokenIndex > -1) {
    data.splice(tokenIndex, 1);
  }

  // Remove from the user interface.
  const tokenNode = list.querySelector<HTMLElement>(`#${accessToken}`);
  if (tokenNode) {
    list.removeChild(tokenNode);
  }

  list.dispatchEvent(
      new CustomEvent('token-removed-for-test', {detail: accessToken}));
}

document.addEventListener('DOMContentLoaded', () => {
  const listEl = document.querySelector<HTMLElement>('#token-list');
  assert(listEl);
  list = listEl;
  sendWithPromise('identityInternalsGetTokens').then(tokens => {
    data = tokens;
    data.forEach(tokenInfo => {
      const item = document.createElement('token-list-item');
      list.appendChild(item);
      item.configure(tokenInfo);
    });
  });

  list.addEventListener('remove-token', e => removeTokenNode(e));
});
