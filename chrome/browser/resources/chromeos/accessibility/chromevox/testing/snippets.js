// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Contains html snippets shared by multiple tests.
 */

function toolbarDoc() {
  return `
   <div tabindex=0 role="toolbar">
     <button>Back</button>
     <button>Forward</button>
   </div>`;
}

function headingDoc() {
  return `
   <h1>World</h1>
   <p>Canada</p>
   <h2>United States</h2>
   <a href="whitehouse.gov">White House</a>`;
}
