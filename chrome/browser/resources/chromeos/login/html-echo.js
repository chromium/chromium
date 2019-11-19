// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'html-echo',

  properties: {content: {type: String, observer: 'contentChanged_'}},

  /**
   * @param {string} content
   * @private
   */
  contentChanged_: function(content) {
    this.innerHTML = content;
  }
});
