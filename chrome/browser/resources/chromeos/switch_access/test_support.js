// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


const TestSupport = {
  /**
   * Connect |parent| and |children| to each other.
   *
   * @param {!Object} parent
   * @param {Array<Object>} children
   */
  setChildren: (parent, children) => {
    // Connect parent to its children.
    parent.children = children;
    parent.firstChild = children[0];
    parent.lastChild = children[children.length - 1];

    for (const i = 0; i < children.length; i++) {
      let child = children[i];

      // Connect children to their parent.
      child.parent = parent;

      // Connect children to each other
      if (i < children.length - 1)
        child.nextSibling = children[i + 1];
      if (i > 0)
        child.previousSibling = children[i - 1];
    }
  },
};
