// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
const s = document.createElement('script');
s.src = chrome.runtime.getURL('embedded_a11y_helper/gdocs_script.js');
document.documentElement.appendChild(s);
})();
