// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const googleRules = {
  'no-cond-assign': 0,
  'no-irregular-whitespace': 2,
  'no-unexpected-multiline': 2,
  'valid-jsdoc': [
    2,
    {
      requireParamDescription: false,
      requireReturnDescription: false,
      requireReturn: false,
      prefer: {returns: 'return'},
    },
  ],
  'curly': [2, 'multi-line'],
  'guard-for-in': 2,
  'no-caller': 2,
  'no-extend-native': 2,
  'no-extra-bind': 2,
  'no-invalid-this': 2,
  'no-multi-spaces': 2,
  'no-multi-str': 2,
  'no-new-wrappers': 2,
  'no-throw-literal': 2,
  'no-with': 2,
  'prefer-promise-reject-errors': 2,
  'no-unused-vars': [2, {args: 'none'}],
  'array-bracket-newline': 0,
  'array-bracket-spacing': [2, 'never'],
  'array-element-newline': 0,
  'block-spacing': [2, 'never'],
  'brace-style': 2,
  'camelcase': [2, {properties: 'never'}],
  'comma-dangle': [2, 'always-multiline'],
  'comma-spacing': 2,
  'comma-style': 2,
  'computed-property-spacing': 2,
  'eol-last': 2,
  'func-call-spacing': 2,
  'indent': [
    'error',
    2,
    {
      'CallExpression': {
        'arguments': 2,
      },
      'FunctionDeclaration': {
        'body': 1,
        'parameters': 2,
      },
      'FunctionExpression': {
        'body': 1,
        'parameters': 2,
      },
      'MemberExpression': 2,
      'ObjectExpression': 1,
      'SwitchCase': 1,
      'ignoredNodes': [
        'ConditionalExpression',
      ],
    },
  ],
  'key-spacing': 2,
  'keyword-spacing': 2,
  'linebreak-style': 2,
  'max-len': [
    2,
    {
      code: 80,
      tabWidth: 2,
      ignoreUrls: true,
      ignorePattern: '^goog.(module|require)',
    },
  ],
  'new-cap': 2,
  'no-array-constructor': 2,
  'no-mixed-spaces-and-tabs': 2,
  'no-multiple-empty-lines': [2, {max: 2}],
  'no-new-object': 2,
  'no-tabs': 2,
  'no-trailing-spaces': 2,
  'object-curly-spacing': 2,
  'one-var': [
    2,
    {
      // Quote the keys to make clang-format format it correctly.
      'var': 'never',
      'let': 'never',
      'const': 'never',
    },
  ],
  'padded-blocks': [2, 'never'],
  'quote-props': [2, 'consistent'],
  'quotes': [2, 'single', {allowTemplateLiterals: true}],
  'require-jsdoc': [
    2,
    {
      require: {
        FunctionDeclaration: true,
        MethodDefinition: true,
        ClassDeclaration: true,
      },
    },
  ],
  'semi': 2,
  'semi-spacing': 2,
  'space-before-blocks': 2,
  'space-before-function-paren': [
    2,
    {
      asyncArrow: 'always',
      anonymous: 'never',
      named: 'never',
    },
  ],
  'spaced-comment': [2, 'always'],

  // It's not available yet in the bundled eslint version in Chrome.
  // 'switch-colon-spacing': 2,

  'arrow-parens': [2, 'always'],
  'constructor-super': 2,
  'generator-star-spacing': [2, 'after'],
  'no-new-symbol': 2,
  'no-this-before-super': 2,
  'no-var': 2,
  'prefer-const': ['error', {destructuring: 'all'}],
  'prefer-rest-params': 2,
  'prefer-spread': 2,
  'rest-spread-spacing': 2,
  'yield-star-spacing': [2, 'after'],
};

/* global module */
module.exports = {
  'root': true,
  'env': {
    'browser': true,
    'es6': true,
    'webextensions': true,
  },
  'parserOptions': {
    'ecmaVersion': 2017,
  },
  'extends': 'eslint:recommended',
  'globals': {
    'arc': 'readable',
    // Adds BigInt64Array here since current version of eslint does not treat
    // BigInt64Array as a defined type.
    'BigInt64Array': 'readable',
    'chromeosCamera': 'readable',
    'cros': 'readable',
    'ImageCapture': 'readable',
    'webkitRequestFileSystem': 'readable',
  },
  // Generally, the rules should be compatible to both bundled and the newest
  // stable eslint, so it's easier to upgrade and develop without the full
  // Chromium tree.
  'rules': Object.assign({}, googleRules, {
    'curly': [2, 'multi-line', 'consistent'],
    'eqeqeq': 2,
    'no-console': [2, {allow: ['warn', 'error']}],

    // We are using 2 spaces before trailing line comments. The option
    // |ignoreEOLComments| is not supported yet in the bundled eslint.
    'no-multi-spaces': 0,

    // The bundled eslint is not smart enough for this. Indentation in the new
    // code should be formatted properly by clang-format, as we required
    // `git cl format --js` before uploading.
    'indent': 0,

    // TODO(shik): temporarily disable the rules we violate (b/117810572).
    'no-redeclare': 0,                  // 5 errors
    'no-var': 0,                        // 273 errors
    'prefer-rest-params': 0,            // 3 errors
    'prefer-const': 0,                  // 17 errors
    'prefer-promise-reject-errors': 0,  // 1 error
  }),
};
