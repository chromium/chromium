// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

module.exports = {
  'ignorePatterns' : ['network_ui/third_party/uPlot.iife.min.d.ts'],
  'rules' : {
    // Turn off since there are too many imports of 'Polymer'. Remove if/when
    // everything under this folder is migrated to PolymerElement.
    'no-restricted-imports' : 'off',

    // Turn off since there are many violations in this folder.
    '@typescript-eslint/consistent-type-imports' : 'off',

    // Turn off until violations in this folder are fixed.
    '@typescript-eslint/ban-ts-comment' : 'off',
  },
};
