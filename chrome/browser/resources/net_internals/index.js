// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MainView} from './main.js';

document.addEventListener('DOMContentLoaded', function() {
  MainView.getInstance();  // from main.js
  const params = new URLSearchParams(window.location.search);
  const test = params.get('test');
  if (test) {
    const script = document.createElement('script');
    script.type = 'module';
    script.src = `chrome://test/net_internals/${test}`;
    document.body.appendChild(script);
  }
});
