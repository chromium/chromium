// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(b:272150598): Investigate sharing this with
// ../embedded_a11y_helper/content.ts, perhaps by renaming cvox_gdocs_script to
// gdoc_script at buildtime for chromevox_helper.
(function() {
const s = document.createElement('script');
s.src = chrome.runtime.getURL('chromevox_helper/cvox_gdocs_script.js');
document.documentElement.appendChild(s);
})();
