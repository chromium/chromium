// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {initialize} from './index.js';
import {loadTestModule} from './test_loader_util.js';

document.addEventListener('DOMContentLoaded', () => {
  // Using a query of "module" provides a hook for the test suite to perform
  // setup actions.
  if (!loadTestModule()) {
    initialize();
  }
});
