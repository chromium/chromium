// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assert} from 'chrome://resources/js/assert.js';
import {sendWithPromise} from 'chrome://resources/js/cr.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';

type NaclInfo = Array<{key: string, value: string}>;

type DomBindElement = HTMLElement&{naclInfo: NaclInfo};

/**
 * Asks the C++ NaClDOMHandler to get details about the NaCl and
 * re-populates the page with the data.
 */
function initialize() {
  sendWithPromise('requestNaClInfo').then((response: {naclInfo: NaclInfo}) => {
    getRequiredElement('loading-message').hidden = true;
    getRequiredElement('body-container').hidden = false;

    const bind = document.body.querySelector<DomBindElement>('dom-bind');
    assert(bind);
    bind.naclInfo = response.naclInfo;
  });
}

document.addEventListener('DOMContentLoaded', initialize);
