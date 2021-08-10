// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DevicePage} from './device_page.js';

function initialize() {
  DevicePage.getInstance();
}

document.addEventListener('DOMContentLoaded', initialize);
