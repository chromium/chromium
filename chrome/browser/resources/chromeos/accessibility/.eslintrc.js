// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

module.exports = {
  'rules' : {
    'arrow-spacing' : ['error'],
    'brace-style' : ['error', '1tbs'],
    'curly' : ['error', 'multi-line', 'consistent'],
    'eqeqeq' : ['error', 'always', {'null' : 'ignore'}],
    'no-confusing-arrow' : ['error'],
    'no-console' : 'off',
    'no-throw-literal' : 'off',
    'no-var' : 'off',
    'object-shorthand' : ['error', 'always'],
    'prefer-const' : ['error', {'destructuring' : 'all'}],
    '@typescript-eslint/explicit-function-return-type' : 'off',
    '@typescript-eslint/naming-convention' : 'off',
  },
  // clang-format off
  'overrides':
    [
      {
        // enable the rule specifically for TypeScript files
        'files': ['*.ts'],
        'rules': {
          '@typescript-eslint/explicit-function-return-type':
            [
              'error',
              {
                'allowExpressions': true,
              },
            ],
            // https://google.github.io/styleguide/jsguide.html#naming
            '@typescript-eslint/naming-convention': [
              'error',
              {
                selector: ['class', 'interface', 'typeAlias', 'enum', 'typeParameter'],
                format: ['StrictPascalCase'],
                filter: {
                  regex: '^(' +
                      // Exclude TypeScript defined interfaces HTMLElementTagNameMap
                      // and HTMLElementEventMap.
                      'HTMLElementTagNameMap|HTMLElementEventMap|' +
                      // Exclude native DOM types which are always named like HTML<Foo>Element.
                      'HTML[A-Za-z]{0,}Element|' +
                      // Exclude native DOM interfaces.
                      'UIEvent|UIEventInit|DOMError|' +
                      // Exclude ISearchUI.
                      'ISearch|ISearchUI|' +
                      // Exclude the SA* classes.
                      'SACache|SACommands|SAChildNode|SANode|SARootNode)$',
                  match: false,
                },
              },
              {
                selector: 'enumMember',
                format: ['UPPER_CASE'],
              },
              {
                selector: 'classMethod',
                format: ['strictCamelCase'],
                modifiers: ['public'],
                filter: {
                  regex: '^(' +
                      // Exclude initialisms such as JSON and IME
                      'toJSON|describeTextChangedByIME|' +
                      // Exclude the short name CVox
                      'isCVoxModifierActive|' +
                      // Exclude the phrase OS.
                      'addOSKeyboardShortcutsMenuItem' +
                      ')$',
                  match: false,
                },
              },
              {
                selector: 'classMethod',
                format: ['strictCamelCase'],
                modifiers: ['private'],
                trailingUnderscore: 'allow',
                filter: {
                  regex: '^createITutorial_$',
                  match: false,
                },
              },
              {
                selector: 'classProperty',
                format: ['UPPER_CASE'],
                modifiers: ['private', 'static', 'readonly'],
              },
              {
                selector: 'classProperty',
                format: ['UPPER_CASE'],
                modifiers: ['public', 'static', 'readonly'],
              },
              {
                selector: 'classProperty',
                format: ['camelCase'],
                modifiers: ['public'],
              },
              {
                selector: 'classProperty',
                format: ['camelCase'],
                modifiers: ['private'],
                trailingUnderscore: 'allow',
              },
              {
                selector: 'parameter',
                format: ['camelCase'],
                leadingUnderscore: 'allow',
              },
              {
                selector: 'function',
                format: ['camelCase'],
              },
            ],
        },
      },
    ],
  // clang-format on
};
