// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

document.addEventListener('DOMContentLoaded', () => {
  const p = document.createElement('p');
  p.textContent = loadTimeData.getString('vcBackgroundTitle');
  document.body.appendChild(p);
});
