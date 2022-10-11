// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

module.exports = {
  'env': {'browser': true, 'es6': true},
  'overrides': [{
    'files': ['**/*.ts'],
    'parser': '../../../../third_party/node/node_modules/@typescript-eslint/parser',
    'plugins': [
      '@typescript-eslint',
    ],
    'rules': {
      '@typescript-eslint/naming-convention': [
        'error',
        // TODO(crbug.com/720034): Remove when StrictPascalCase is rolled out to
        // the default configuration.
        {
          selector: ['class', 'interface', 'typeAlias', 'enum', 'typeParameter'],
          format: ['StrictPascalCase'],
          filter: {
            regex: 'HTMLElementTagNameMap|HTMLElementEventMap',
            match: false,
          },
        },
      ],
    },
  }],
};
