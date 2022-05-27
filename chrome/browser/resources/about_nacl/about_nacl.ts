// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assert} from 'chrome://resources/js/assert_ts.js';
import {sendWithPromise} from 'chrome://resources/js/cr.m.js';
import {$} from 'chrome://resources/js/util.m.js';

type NaclInfo = Array<{key: string, value: string}>;

type DomBindElement = HTMLElement&{naclInfo: NaclInfo};

/**
 * Asks the C++ NaClDOMHandler to get details about the NaCl and
 * re-populates the page with the data.
 */
function initialize() {
  sendWithPromise('requestNaClInfo').then((response: {naclInfo: NaclInfo}) => {
    $('loading-message').hidden = true;
    $('body-container').hidden = false;

    const bind = document.body.querySelector<DomBindElement>('dom-bind');
    assert(bind);
    bind.naclInfo = response.naclInfo;
  });
}

document.addEventListener('DOMContentLoaded', initialize);
