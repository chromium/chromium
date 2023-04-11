// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

module.exports = {
  'rules' : {
    'comma-dangle' : ['error', 'always-multiline'],
    'no-console' : 'off',

    // Turn off since there are too many imports of 'Polymer'. Remove if/when
    // everything under this folder is migrated to PolymerElement.
    'no-restricted-imports' : 'off',
  },
};
