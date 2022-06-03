// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/js/jstemplate_compiled.js';

import {sendWithPromise} from 'chrome://resources/js/cr.m.js';
import {$} from 'chrome://resources/js/util.m.js';

document.addEventListener('DOMContentLoaded', function() {
  sendWithPromise('updateData').then(data => {
    jstProcess(new JsEvalContext(data), $('template'));
  });
});
