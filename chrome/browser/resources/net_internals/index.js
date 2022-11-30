// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MainView} from './main.js';
import {loadTestModule} from './test_loader_util.js';

document.addEventListener('DOMContentLoaded', function() {
  MainView.getInstance();  // from main.js
  loadTestModule();
});
