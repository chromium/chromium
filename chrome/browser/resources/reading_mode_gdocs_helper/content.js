// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(b:272150598): Investigate sharing this with
// ../embedded_a11y_helper/content.ts.
(function() {
const s = document.createElement('script');
s.src = chrome.runtime.getURL('gdocs_script.js');
document.documentElement.appendChild(s);
})();
