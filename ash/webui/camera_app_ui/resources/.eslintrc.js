// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is also checked by ESLint, and some of the property in the
// settings doesn't follow naming convention.
/* eslint-disable @typescript-eslint/naming-convention */
// From
// https://github.com/google/eslint-config-google/blob/b8ba12f58a4d71ee1f66b504a59bfe8de381ab4b/index.js#L20
const googleRules = {
  // The rules below are listed in the order they appear on the eslint
  // rules page. All rules are listed to make it easier to keep in sync
  // as new ESLint rules are added.
  // http://eslint.org/docs/rules/
  // - Rules in the `eslint:recommended` ruleset that aren't specifically
  //   mentioned by the google styleguide are listed but commented out (so
  //   they don't override a base ruleset).
  // - Rules that are recommended but contradict the Google styleguide
  //   are explicitly set to the Google styleguide value.

  // Possible Errors
  // http://eslint.org/docs/rules/#possible-errors
  // ---------------------------------------------
  // 'for-direction': 'off',
  // 'no-await-in-loop': 'off',
  // 'no-compare-neg-zero': 'error', // eslint:recommended
  'no-cond-assign': 'off',  // eslint:recommended
  // 'no-console': 'error', // eslint:recommended
  // 'no-constant-condition': 'error', // eslint:recommended
  // 'no-control-regex': 'error', // eslint:recommended
  // 'no-debugger': 'error', // eslint:recommended
  // 'no-dupe-args': 'error', // eslint:recommended
  // 'no-dupe-keys': 'error', // eslint:recommended
  // 'no-duplicate-case': 'error', // eslint:recommended
  // 'no-empty': 'error', // eslint:recommended
  // 'no-empty-character-class': 'error', // eslint:recommended
  // 'no-ex-assign': 'error', // eslint:recommended
  // 'no-extra-boolean-cast': 'error', // eslint:recommended
  // 'no-extra-parens': 'off',
  // 'no-extra-semi': 'error', // eslint:recommended
  // 'no-func-assign': 'error', // eslint:recommended
  // 'no-inner-declarations': 'error', // eslint:recommended
  // 'no-invalid-regexp': 'error', // eslint:recommended
  'no-irregular-whitespace': 'error',  // eslint:recommended
  // 'no-obj-calls': 'error', // eslint:recommended
  // 'no-prototype-builtins': 'off',
  // 'no-regex-spaces': 'error', // eslint:recommended
  // 'no-sparse-arrays': 'error', // eslint:recommended
  // 'no-template-curly-in-string': 'off',
  'no-unexpected-multiline': 'error',  // eslint:recommended
  // 'no-unreachable': 'error', // eslint:recommended
  // 'no-unsafe-finally': 'error', // eslint:recommended
  // 'no-unsafe-negation': 'off',
  // 'use-isnan': 'error' // eslint:recommended
  'valid-jsdoc': [
    'error',
    {
      requireParamDescription: false,
      requireReturnDescription: false,
      requireReturn: false,
      prefer: {returns: 'return'},
    },
  ],
  // 'valid-typeof': 'error' // eslint:recommended


  // Best Practices
  // http://eslint.org/docs/rules/#best-practices
  // --------------------------------------------

  // 'accessor-pairs': 'off',
  // 'array-callback-return': 'off',
  // 'block-scoped-var': 'off',
  // 'class-methods-use-this': 'off',
  // 'complexity': 'off',
  // 'consistent-return': 'off'
  // TODO(philipwalton): add an option to enforce braces with the
  // exception of simple, single-line if statements.
  'curly': ['error', 'multi-line'],
  // 'default-case': 'off',
  // 'dot-location': 'off',
  // 'dot-notation': 'off',
  // 'eqeqeq': 'off',
  'guard-for-in': 'error',
  // 'no-alert': 'off',
  'no-caller': 'error',
  // 'no-case-declarations': 'error', // eslint:recommended
  // 'no-div-regex': 'off',
  // 'no-else-return': 'off',
  // 'no-empty-function': 'off',
  // 'no-empty-pattern': 'error', // eslint:recommended
  // 'no-eq-null': 'off',
  // 'no-eval': 'off',
  'no-extend-native': 'error',
  'no-extra-bind': 'error',
  // 'no-extra-label': 'off',
  // 'no-fallthrough': 'error', // eslint:recommended
  // 'no-floating-decimal': 'off',
  // 'no-global-assign': 'off',
  // 'no-implicit-coercion': 'off',
  // 'no-implicit-globals': 'off',
  // 'no-implied-eval': 'off',
  'no-invalid-this': 'error',
  // 'no-iterator': 'off',
  // 'no-labels': 'off',
  // 'no-lone-blocks': 'off',
  // 'no-loop-func': 'off',
  // 'no-magic-numbers': 'off',
  'no-multi-spaces': 'error',
  'no-multi-str': 'error',
  // 'no-new': 'off',
  // 'no-new-func': 'off',
  'no-new-wrappers': 'error',
  // 'no-octal': 'error', // eslint:recommended
  // 'no-octal-escape': 'off',
  // 'no-param-reassign': 'off',
  // 'no-proto': 'off',
  // 'no-redeclare': 'error', // eslint:recommended
  // 'no-restricted-properties': 'off',
  // 'no-return-assign': 'off',
  // 'no-script-url': 'off',
  // 'no-self-assign': 'error', // eslint:recommended
  // 'no-self-compare': 'off',
  // 'no-sequences': 'off',
  'no-throw-literal': 'error',  // eslint:recommended
  // 'no-unmodified-loop-condition': 'off',
  // 'no-unused-expressions': 'off',
  // 'no-unused-labels': 'error', // eslint:recommended
  // 'no-useless-call': 'off',
  // 'no-useless-concat': 'off',
  // 'no-useless-escape': 'off',
  // 'no-void': 'off',
  // 'no-warning-comments': 'off',
  'no-with': 'error',
  'prefer-promise-reject-errors': 'error',
  // 'radix': 'off',
  // 'require-await': 'off',
  // 'vars-on-top': 'off',
  // 'wrap-iife': 'off',
  // 'yoda': 'off',

  // Strict Mode
  // http://eslint.org/docs/rules/#strict-mode
  // -----------------------------------------
  // 'strict': 'off',

  // Variables
  // http://eslint.org/docs/rules/#variables
  // ---------------------------------------
  // 'init-declarations': 'off',
  // 'no-catch-shadow': 'off',
  // 'no-delete-var': 'error', // eslint:recommended
  // 'no-label-var': 'off',
  // 'no-restricted-globals': 'off',
  // 'no-shadow': 'off',
  // 'no-shadow-restricted-names': 'off',
  // 'no-undef': 'error', // eslint:recommended
  // 'no-undef-init': 'off',
  // 'no-undefined': 'off',
  'no-unused-vars': ['error', {args: 'none'}],  // eslint:recommended
  // 'no-use-before-define': 'off',

  // Node.js and CommonJS
  // http://eslint.org/docs/rules/#nodejs-and-commonjs
  // -------------------------------------------------
  // 'callback-return': 'off',
  // 'global-require': 'off',
  // 'handle-callback-err': 'off',
  // 'no-buffer-constructor': 'off',
  // 'no-mixed-requires': 'off',
  // 'no-new-require': 'off',
  // 'no-path-concat': 'off',
  // 'no-process-env': 'off',
  // 'no-process-exit': 'off',
  // 'no-restricted-modules': 'off',
  // 'no-sync': 'off',

  // Stylistic Issues
  // http://eslint.org/docs/rules/#stylistic-issues
  // ----------------------------------------------
  'array-bracket-newline': 'off',  // eslint:recommended
  'array-bracket-spacing': ['error', 'never'],
  'array-element-newline': 'off',  // eslint:recommended
  'block-spacing': ['error', 'never'],
  'brace-style': 'error',
  'camelcase': ['error', {properties: 'never'}],
  // 'capitalized-comments': 'off',
  'comma-dangle': ['error', 'always-multiline'],
  'comma-spacing': 'error',
  'comma-style': 'error',
  'computed-property-spacing': 'error',
  // 'consistent-this': 'off',
  'eol-last': 'error',
  'func-call-spacing': 'error',
  // 'func-name-matching': 'off',
  // 'func-names': 'off',
  // 'func-style': 'off',
  // 'id-denylist': 'off',
  // 'id-length': 'off',
  // 'id-match': 'off',
  'indent': [
    'error',
    2,
    {
      CallExpression: {
        arguments: 2,
      },
      FunctionDeclaration: {
        body: 1,
        parameters: 2,
      },
      FunctionExpression: {
        body: 1,
        parameters: 2,
      },
      MemberExpression: 2,
      ObjectExpression: 1,
      SwitchCase: 1,
      ignoredNodes: [
        'ConditionalExpression',
      ],
    },
  ],
  // 'jsx-quotes': 'off',
  'key-spacing': 'error',
  'keyword-spacing': 'error',
  // 'line-comment-position': 'off',
  'linebreak-style': 'error',
  // 'lines-around-comment': 'off',
  // 'max-depth': 'off',
  'max-len': [
    'error',
    {
      code: 80,
      tabWidth: 2,
      ignoreUrls: true,
      ignorePattern: 'goog.(module|require)',
    },
  ],
  // 'max-lines': 'off',
  // 'max-nested-callbacks': 'off',
  // 'max-params': 'off',
  // 'max-statements': 'off',
  // 'max-statements-per-line': 'off',
  // TODO(philipwalton): add a rule to enforce the operator appearing
  // at the end of the line.
  // 'multiline-ternary': 'off',
  'new-cap': 'error',
  // 'new-parens': 'off',
  // 'newline-per-chained-call': 'off',
  'no-array-constructor': 'error',
  // 'no-bitwise': 'off',
  // 'no-continue': 'off',
  // 'no-inline-comments': 'off',
  // 'no-lonely-if': 'off',
  // 'no-mixed-operators': 'off',
  'no-mixed-spaces-and-tabs': 'error',  // eslint:recommended
  // 'no-multi-assign': 'off',
  'no-multiple-empty-lines': ['error', {max: 2}],
  // 'no-negated-condition': 'off',
  // 'no-nested-ternary': 'off',
  'no-new-object': 'error',
  // 'no-plusplus': 'off',
  // 'no-restricted-syntax': 'off',
  'no-tabs': 'error',
  // 'no-ternary': 'off',
  'no-trailing-spaces': 'error',
  // 'no-underscore-dangle': 'off',
  // 'no-unneeded-ternary': 'off',
  // 'no-whitespace-before-property': 'off',
  // 'nonblock-statement-body-position': 'off',
  // 'object-curly-newline': 'off',
  'object-curly-spacing': 'error',
  // 'object-property-newline': 'off',
  'one-var': [
    'error',
    {
      // Quote the keys to make clang-format format it correctly.
      /* eslint-disable quote-props */
      'var': 'never',
      'let': 'never',
      'const': 'never',
      /* eslint-enable quote-props */
    },
  ],
  // 'one-var-declaration-per-line': 'off',
  // 'operator-assignment': 'off',
  'operator-linebreak': ['error', 'after'],
  'padded-blocks': ['error', 'never'],
  // 'padding-line-between-statements': 'off',
  'quote-props': ['error', 'consistent'],
  'quotes': ['error', 'single', {allowTemplateLiterals: true}],
  'require-jsdoc': [
    'error',
    {
      require: {
        FunctionDeclaration: true,
        MethodDefinition: true,
        ClassDeclaration: true,
      },
    },
  ],
  'semi': 'error',
  'semi-spacing': 'error',
  // 'semi-style': 'off',
  // 'sort-keys': 'off',
  // 'sort-vars': 'off',
  'space-before-blocks': 'error',
  'space-before-function-paren': [
    'error',
    {
      asyncArrow: 'always',
      anonymous: 'never',
      named: 'never',
    },
  ],
  // 'space-in-parens': 'off',
  // 'space-infix-ops': 'off',
  // 'space-unary-ops': 'off',
  'spaced-comment': ['error', 'always'],
  'switch-colon-spacing': 'error',
  // 'template-tag-spacing': 'off',
  // 'unicode-bom': 'off',
  // 'wrap-regex': 'off',

  // ECMAScript 6
  // http://eslint.org/docs/rules/#ecmascript-6
  // ------------------------------------------
  // 'arrow-body-style': 'off',
  // TODO(philipwalton): technically arrow parens are optional but
  // recommended. ESLint doesn't support a *consistent* setting so
  // "always" is used.
  'arrow-parens': ['error', 'always'],
  // 'arrow-spacing': 'off',
  'constructor-super': 'error',  // eslint:recommended
  'generator-star-spacing': ['error', 'after'],
  // 'no-class-assign': 'off',
  // 'no-confusing-arrow': 'off',
  // 'no-const-assign': 'off', // eslint:recommended
  // 'no-dupe-class-members': 'off', // eslint:recommended
  // 'no-duplicate-imports': 'off',
  'no-new-symbol': 'error',  // eslint:recommended
  // 'no-restricted-imports': 'off',
  'no-this-before-super': 'error',  // eslint:recommended
  // 'no-useless-computed-key': 'off',
  // 'no-useless-constructor': 'off',
  // 'no-useless-rename': 'off',
  'no-var': 'error',
  // 'object-shorthand': 'off',
  // 'prefer-arrow-callback': 'off',
  'prefer-const': ['error', {destructuring: 'all'}],
  // 'prefer-destructuring': 'off',
  // 'prefer-numeric-literals': 'off',
  'prefer-rest-params': 'error',
  'prefer-spread': 'error',
  // 'prefer-template': 'off',
  // 'require-yield': 'error', // eslint:recommended
  'rest-spread-spacing': 'error',
  // 'sort-imports': 'off',
  // 'symbol-description': 'off',
  // 'template-curly-spacing': 'off',
  'yield-star-spacing': ['error', 'after'],
};

// https://github.com/eslint/eslint/issues/8769
// Hack node module system so that eslint-plugin-cca resolves to local module.
/* global require */
/* eslint-disable-next-line @typescript-eslint/no-var-requires */
const m = require('module');
const originalResolve = m._resolveFilename;
m._resolveFilename = (request, ...args) => {
  if (request === 'eslint-plugin-cca') {
    return require.resolve('./eslint_plugin');
  } else {
    return originalResolve.call(m, request, ...args);
  }
};

const typescriptEslintDir =
    '../../../../third_party/node/node_modules/@typescript-eslint';

/* global module */
module.exports = {
  root: true,
  env: {
    browser: true,
    es2020: true,
    webextensions: true,
  },
  parserOptions: {
    ecmaVersion: 2020,
    sourceType: 'module',
  },
  extends: [
    'eslint:recommended',
    'plugin:@typescript-eslint/recommended',
  ],
  settings: {
    jsdoc: {
      tagNamePreference: {
        returns: 'return',
        // go/tsstyle#omit-comments-that-are-redundant-with-typescript
        default: false,
        enum: false,
        implements: false,
        interface: false,
        override: false,
        private: false,
        protected: false,
        template: false,
        type: false,
        typedef: false,
      },
    },
  },
  parser: `${typescriptEslintDir}/parser`,
  plugins: ['@typescript-eslint', 'jsdoc', 'eslint-plugin-cca'],
  // Generally, the rules should be compatible to both bundled and the newest
  // stable eslint, so it's easier to upgrade and develop without the full
  // Chromium tree.
  rules: Object.assign({}, googleRules, {
    'curly': ['error', 'multi-line', 'consistent'],
    'eqeqeq': 'error',
    'no-console': ['error', {allow: ['warn', 'error']}],
    'no-multi-spaces': ['error', {ignoreEOLComments: true}],

    // The bundled eslint is not smart enough for this. Indentation in the new
    // code should be formatted properly by clang-format, as we required
    // `git cl format --js` before uploading.
    'indent': 'off',

    // To resolve the conflict with clang-format.
    'generator-star-spacing': [
      'error',
      {
        named: 'after',
        anonymous: 'neither',
        method: 'both',
      },
    ],

    // This doesn't work well with TypeScript files. The alternate
    // @typescript-eslint/no-unused-vars that works with TypeScript files is
    // enabled in @typescript-eslint/recommended.
    'no-unused-vars': 'off',

    // Use eslint-plugin-jsdoc instead of ESLint builtin valid-jsdoc /
    // require-jsdoc, since it has better flexibility on not requiring types
    // for jsdoc, and is also recommended on the ESLint rule page.
    'valid-jsdoc': 'off',
    'require-jsdoc': 'off',

    // This is not useful since ES6 and contradicts to
    // go/tsstyle#function-declarations.
    'no-inner-declarations': 'off',

    // go/tsstyle#omit-comments-that-are-redundant-with-typescript
    'jsdoc/no-types': 'error',
    'jsdoc/require-jsdoc': [
      'error',
      {
        publicOnly: true,
      },
    ],
    'jsdoc/require-param': 'off',
    'jsdoc/require-param-type': 'off',
    'jsdoc/require-returns': 'off',
    'jsdoc/require-returns-type': 'off',
    'jsdoc/require-yields': 'off',

    'jsdoc/multiline-blocks': [
      'error',
      {
        noSingleLineBlocks: true,
      },
    ],
    'jsdoc/no-bad-blocks': 'error',
    'jsdoc/no-defaults': 'error',
    'jsdoc/no-multi-asterisks': [
      'error',
      {
        allowWhitespace: true,
      },
    ],
    'jsdoc/require-asterisk-prefix': 'error',
    'jsdoc/require-description-complete-sentence': [
      'error',
      {
        abbreviations: ['e.g.'],
      },
    ],

    // go/tsstyle states that no variable should have _ as prefix/suffix, but
    // there's no better alternative for unused function parameters. Since the
    // convention for noUnusedParameters for TypeScript is also leading
    // underscore, we use the same ignore pattern here.  See b/173108529 and
    // g/typescript-style/uOfKsoxxWEY/HCgzNfAFAwAJ for other discussions.
    '@typescript-eslint/no-unused-vars': [
      'error',
      {
        varsIgnorePattern: '^_',
        argsIgnorePattern: '^_',
      },
    ],

    'no-restricted-syntax': [
      'error',
      // Disallow parseInt. (go/tsstyle#type-coercion)
      {
        selector: 'CallExpression[callee.name="parseInt"]',
        message: 'parseInt are not allowed, use Number() instead. ' +
            '(go/tsstyle#type-coercion)',
      },
      // Disallow Array constructor. (go/tsstyle#array-constructor)
      {
        selector: 'NewExpression[callee.name="Array"], ' +
            'CallExpression[callee.name="Array"]',
        message: 'Array constructor are not allowed. ' +
            '(go/tsstyle#array-constructor)',
      },
      // Disallow calling Error without new. (go/tsstyle#exceptions)
      {
        selector: 'CallExpression[callee.name="Error"]',
        message: 'Error constructor should be called with new Error(...). ' +
            '(go/tsstyle#exceptions)',
      },
      // Disallow for (... in ...). (go/tsstyle#iterating-objects)
      {
        selector: 'ForInStatement',
        message: 'for (... in ...) is not allowed. ' +
            '(go/tsstyle#iterating-objects)',
      },
      // Disallow 'Interface' as identifier suffix. (go/tsstyle#naming-style)
      {
        selector: 'Identifier[name=/.*Interface/]',
        message: 'Don\'t use "Interface" as identifier suffix. ' +
            '(go/tsstyle#naming-style)',
      },
      // Disallow forEach. (go/tsjs-practices/iteration)
      // TODO(pihsun): This was relaxed in style guide in cl/430720959,
      // consider relaxing this if there's place where forEach makes the code
      // much simpler.
      {
        selector: 'CallExpression[callee.property.name="forEach"]',
        message: 'forEach are not allowed. (go/tsstyle#iterating-containers)',
      },
      // Disallow function() {...}. (go/tsstyle#function-declarations)
      {
        selector: ':not(:matches(MethodDefinition, Property))' +
            ' > FunctionExpression:not([id])',
        message: 'Use named function or arrow function instead. ' +
            '(go/tsstyle#function-declarations)',
      },
      // Disallow local function declaration with arrow function without
      // accessing this. This might have some false negative if the "this" is
      // accessed deep inside the function in another scope, but should be
      // rare. (go/tsstyle#function-declarations)
      {
        selector: 'VariableDeclarator:not(:has(.id[typeAnnotation]))' +
            ' > ArrowFunctionExpression.init:not(:has(ThisExpression))',
        message: 'Use named function to declare local function. ' +
            '(go/tsstyle#function-declarations)',
      },
      // Disallow private fields. (go/tsstyle#private-fields)
      {
        selector: 'TSPrivateIdentifier',
        message: 'Private fields are not allowed. (go/tsstyle#private-fields)',
      },
      // Disallow explicit boolean coercions in condition.
      // (go/tsstyle#type-coercion-implicit)
      {
        selector: ':matches(IfStatement, WhileStatement)' +
            ' > UnaryExpression.test[operator="!"]' +
            ' > UnaryExpression.argument[operator="!"]',
        message: 'Explicit boolean coercion is not needed in conditions. ' +
            '(go/tsstyle#type-coercion-implicit)',
      },
    ],

    '@typescript-eslint/naming-convention': [
      'error',
      {
        selector: 'default',
        format: ['camelCase'],
      },
      {
        selector: 'variable',
        format: ['camelCase', 'UPPER_CASE'],
      },
      {
        selector: 'typeLike',
        format: ['PascalCase'],
      },
      {
        selector: 'enumMember',
        format: ['UPPER_CASE'],
      },
      {
        selector: 'parameter',
        modifiers: ['unused'],
        format: ['camelCase'],
        leadingUnderscore: 'allow',
      },
    ],

    // This is covered by @typescript-eslint/naming-convention.
    'camelcase': 'off',

    // go/tsstyle#arrayt-type
    '@typescript-eslint/array-type': [
      'error',
      {
        default: 'array-simple',
      },
    ],

    // go/tsstyle#type-assertions-syntax
    // go/tsstyle#type-assertions-and-object-literals
    '@typescript-eslint/consistent-type-assertions': [
      'error',
      {
        assertionStyle: 'as',
        objectLiteralTypeAssertions: 'never',
      },
    ],

    // go/tsstyle#interfaces-vs-type-aliases
    '@typescript-eslint/consistent-type-definitions': ['error', 'interface'],

    // go/tsstyle#import-export-type
    '@typescript-eslint/consistent-type-imports': [
      'error',
      {
        prefer: 'no-type-imports',
      },
    ],

    'quote-props': ['error', 'consistent-as-needed'],

    // go/tsstyle#visibility
    '@typescript-eslint/explicit-member-accessibility': [
      'error',
      {
        accessibility: 'no-public',
      },
    ],

    // go/tsstyle#member-property-declarations
    '@typescript-eslint/member-delimiter-style': [
      'error',
      {
        multiline: {
          delimiter: 'comma',
          requireLast: true,
        },
        singleline: {
          delimiter: 'comma',
          requireLast: false,
        },
        multilineDetection: 'last-member',
        overrides: {
          interface: {
            multiline: {
              delimiter: 'semi',
              requireLast: true,
            },
            singleline: {
              delimiter: 'semi',
              requireLast: true,
            },
          },
        },
      },
    ],

    '@typescript-eslint/prefer-optional-chain': 'error',

    '@typescript-eslint/sort-type-union-intersection-members': 'error',

    'comma-dangle': 'off',
    '@typescript-eslint/comma-dangle': ['error', 'always-multiline'],

    'func-call-spacing': 'off',
    '@typescript-eslint/func-call-spacing': 'error',

    '@typescript-eslint/lines-between-class-members': 'error',

    '@typescript-eslint/no-unused-expressions': 'error',

    'cca/parameter-comment-format': 'error',

    'cca/generic-parameter-on-declaration-type': 'error',

    'cca/todo-format': 'error',

    // go/tsstyle#constructors
    'new-parens': 'error',

    // go/tsstyle#assignment-in-control-statements
    'no-cond-assign': 'error',

    // go/tsstyle#switch-statements
    'default-case': 'error',

    // go/tsstyle#return-types
    '@typescript-eslint/explicit-module-boundary-types': 'error',

    // Upgrade several warning in @typescript-eslint/recommended to error,
    // since there's no easy way to tell eslint to stop on all warning in
    // config file.
    '@typescript-eslint/no-explicit-any': 'error',
    '@typescript-eslint/no-non-null-assertion': 'error',

    // The remaining of jsdoc/recommended, with severity changed to error.
    // Since there's no easy way to tell eslint to stop on all warning in
    // config file, we manually copied all rules here.
    'jsdoc/check-access': 'error',
    'jsdoc/check-alignment': 'error',
    'jsdoc/check-param-names': 'error',
    'jsdoc/check-property-names': 'error',
    'jsdoc/check-tag-names': 'error',
    'jsdoc/check-types': 'error',
    'jsdoc/check-values': 'error',
    'jsdoc/empty-tags': 'error',
    'jsdoc/implements-on-classes': 'error',
    'jsdoc/newline-after-description': 'error',
    'jsdoc/no-undefined-types': 'error',
    'jsdoc/require-param-description': 'error',
    'jsdoc/require-param-name': 'error',
    'jsdoc/require-property': 'error',
    'jsdoc/require-property-description': 'error',
    'jsdoc/require-property-name': 'error',
    'jsdoc/require-property-type': 'error',
    'jsdoc/require-returns-check': 'error',
    'jsdoc/require-returns-description': 'error',
    'jsdoc/require-yields-check': 'error',
    'jsdoc/tag-lines': 'error',
    'jsdoc/valid-types': 'error',
  }),
  overrides: [{
    files: ['**/*.ts'],
    parserOptions: {
      // eslint-disable-next-line no-undef
      tsconfigRootDir: __dirname,
      project: './tsconfig_base.json',
    },
    rules: {
      // go/tsstyle#use-readonly
      '@typescript-eslint/prefer-readonly': 'error',

      '@typescript-eslint/require-array-sort-compare': 'error',

      '@typescript-eslint/prefer-nullish-coalescing': 'error',

      // go/tsstyle#optimization-compatibility-for-property-access
      '@typescript-eslint/dot-notation': 'error',

      '@typescript-eslint/return-await': 'error',
    },
  }],
};
