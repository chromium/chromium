// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

module.exports = {
  'rules': {
    'brace-style': ['error', '1tbs'],
    'curly': ['error', 'multi-line', 'consistent'],
    'eqeqeq': ['error', 'always', {'null': 'ignore'}],
    'no-console': 'off',
    'no-throw-literal': 'off',
    'object-shorthand': ['error', 'always'],
    'prefer-const': ['error', {'destructuring': 'all'}],
  },
};
