// Copyright 2020 The Chromium Authors. All Rights Reserved.
// Use of this source code is governed by the Apache v2.0 license that can be
// found in the LICENSE file.

import './strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {$} from 'chrome://resources/js/util.m.js';

function initialize() {
  for (const emoji of loadTimeData.getString('emoji').split(',')) {
    const block = document.createElement('div');
    block.innerText = emoji;
    $('emoji-picker').appendChild(block);
  }
}


document.addEventListener('DOMContentLoaded', initialize);