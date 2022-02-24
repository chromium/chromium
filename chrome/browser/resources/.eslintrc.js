// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

module.exports = {
  'env': {
    'browser': true,
    'es6': true,
  },

  'overrides': [{
    'files': ['**/*.ts'],
    'parser':
        '../../../third_party/node/node_modules/@typescript-eslint/parser',
    'plugins': [
      '@typescript-eslint',
    ],
    'rules': {
      'no-unused-vars': 'off',
      '@typescript-eslint/no-unused-vars': [
        'error', {
          argsIgnorePattern: '^_',
          varsIgnorePattern: '^_',
        }
      ]
    }
  }]
};
