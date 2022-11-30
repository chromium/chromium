// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

module.exports = {
  'rules': {
    // Override restrictions for document.getElementById usage since,
    // chrome://resources/ash/common/util.js is not accessible for chromevox.
    'no-restricted-properties': 'off',
  },
};
