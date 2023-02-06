// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const config = require('./.eslintrc.js');

const tsOverride = config['overrides'][0];
tsOverride.rules = {
  ...tsOverride.rules,
  '@typescript-eslint/no-floating-promises': 'error',
  '@typescript-eslint/require-await': 'error',
  '@typescript-eslint/await-thenable': 'error',
  '@typescript-eslint/no-meaningless-void-operator': 'error',
};

module.exports = config;
