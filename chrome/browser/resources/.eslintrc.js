// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

module.exports = {
  'env' : {
    'browser' : true,
    'es6' : true,
  },
  'rules' : {
    'eqeqeq' : ['error', 'always', {'null' : 'ignore'}],
  },

  'overrides': [{
    'files': ['**/*.ts'],
    'parser': '../../../third_party/node/node_modules/@typescript-eslint/parser',
    'plugins': [
      '@typescript-eslint',
    ],
    'rules': {
      // https://google.github.io/styleguide/tsguide.html#interfaces-vs-type-aliases
      '@typescript-eslint/consistent-type-definitions' : ['error', 'interface'],
    },
  }],
};
