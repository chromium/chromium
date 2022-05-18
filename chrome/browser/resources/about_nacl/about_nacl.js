// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {sendWithPromise} from 'chrome://resources/js/cr.m.js';
import {$} from 'chrome://resources/js/util.m.js';

/**
 * Asks the C++ NaClDOMHandler to get details about the NaCl and
 * re-populates the page with the data.
 */
function initialize() {
  sendWithPromise('requestNaClInfo').then((moduleListData) => {
    $('loading-message').hidden = true;
    $('body-container').hidden = false;

    /**
     * Takes the |moduleListData| input argument which represents data about
     * the currently available modules and populates the HTML template
     * with that data.
     */
    const bind = document.body.querySelector('dom-bind');
    bind.naclInfo = moduleListData.naclInfo;
  });
}

document.addEventListener('DOMContentLoaded', initialize);
