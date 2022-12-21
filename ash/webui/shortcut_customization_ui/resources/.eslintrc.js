// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Disable clang-format because it produces odd formatting.
// clang-format off
module.exports = {
    'parserOptions': {
      'project': ['./tsconfig_base.json'],
    },
    'rules': {
      '@typescript-eslint/explicit-function-return-type': ['error'],
    },
    'overrides': [{
      'files': ['**/*.ts'],
      'parser':
        '../../../../third_party/node/node_modules/@typescript-eslint/parser',
      'parserOptions': {
        tsconfigRootDir: __dirname,
      },
      'plugins': ['@typescript-eslint'],
      'rules': {
        '@typescript-eslint/naming-convention':
          ['error',
            {
              selector: ['classMethod', 'classProperty'],
              format: ['camelCase'],
              modifiers: ['private'],
              trailingUnderscore: 'forbid',
            },
          ],
      },
    }],
  };
// clang-format on