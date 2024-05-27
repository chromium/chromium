// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(b/342057600): Fetch iframe target from the browser process.
const MALL_ORIGIN = 'https://discover.apps.chrome/';

const mallFrame = document.createElement('iframe');
mallFrame.src = MALL_ORIGIN;
document.body.appendChild(mallFrame);
