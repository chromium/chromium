// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  const metaTags = document.getElementsByTagName('meta');
  let output = {};
  for (let i = 0; i < metaTags.length; i++) {
    let curMeta = metaTags[i];
    let name = curMeta.getAttribute("property");
    let value = curMeta.getAttribute("content");
    if (!name || !value) continue;
    // "og" in this context refers to "open graph" rather than "optimization
    // guide".
    if (name.startsWith("og:")) {
      output[name.substring(3)] = value;
    }
  }
  return JSON.stringify(output);
})();

