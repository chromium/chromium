// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const ActiveDescendantSite = `
  <span tabindex="1">Top</span>
  <div id="group" role="group" style="width: 200px"
      aria-activedescendant="apple">
    <div id="apple" role="treeitem">Apple</div>
    <div id="banana" role="treeitem" style="margin-top: 400px">
      Banana
    </div>
  </div>
  <script>
    const group = document.getElementById('group');
    group.addEventListener('click', function() {
      group.setAttribute('aria-activedescendant', 'banana');
    });
  </script>
`;
