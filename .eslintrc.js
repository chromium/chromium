// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

module.exports = {
  'root': true,
  'env': {
    'browser': true,
    'es6': true,
  },
  'parserOptions': {
    'ecmaVersion': 2017,
    'sourceType': 'module',
  },
  'rules': {
    // Enabled checks.
    'brace-style': ['error', '1tbs'],
    'curly': ['error', 'multi-line', 'consistent'],
    'no-extra-boolean-cast': 'error',
    'no-extra-semi': 'error',
    'no-new-wrappers': 'error',
    'no-restricted-properties': [
      'error',
      {
        'object': 'document',
        'property': 'getElementById',
        'message': 'Use $(\'id\') or getSVGElement(\'id\') ' +
            'from chrome://resources/js/util.js instead of ' +
            'document.getElementById(\'id\')',
      },
      {
        'property': '__lookupGetter__',
        'message': 'Use Object.getOwnPropertyDescriptor',
      },
      {
        'property': '__lookupSetter__',
        'message': 'Use Object.getOwnPropertyDescriptor',
      },
      {
        'property': '__defineGetter__',
        'message': 'Use Object.defineProperty',
      },
      {
        'property': '__defineSetter__',
        'message': 'Use Object.defineProperty',
      },
    ],
    'semi': ['error', 'always'],

    // TODO(dpapad): Add more checks according to our styleguide.
  },
};
