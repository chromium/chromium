// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: ESLint Config inspector (https://github.com/eslint/config-inspector)
// can be used to find duplicate / conflict rules.

// This file is also checked by ESLint, and some of the property in the
// settings doesn't follow naming convention.
/* eslint-disable @typescript-eslint/naming-convention */

/* eslint-disable @stylistic/max-len */

import eslint from
  '../../../../third_party/node/node_modules/@eslint/js/src/index.js';
import stylistic from
  '../../../../third_party/node/node_modules/@stylistic/eslint-plugin/dist/index.js';
// TODO(pihsun): Using the @typescript-eslint package is a bit awkward now
// since we don't have the new typescript-eslint (without the @) package that
// supports flat config better, so we need to glue it by ourselves. Use the new
// package when it's available in Chromium.
import tseslintOverride from
  '../../../../third_party/node/node_modules/@typescript-eslint/eslint-plugin/dist/configs/eslint-recommended-raw.js';
import tseslint from
  '../../../../third_party/node/node_modules/@typescript-eslint/eslint-plugin/dist/index.js';
import tseslintParser from
  '../../../../third_party/node/node_modules/@typescript-eslint/parser/dist/index.js';
import jsdoc from
  '../../../../third_party/node/node_modules/eslint-plugin-jsdoc/dist/index.cjs';

/* eslint-enable @stylistic/max-len */

import cra from './scripts/eslint_plugin/index.mjs';

// This is the rules at the root .eslintrc.js, copied at commit
// a5013fa3b4980a473f187dc6aedd97fc7577af79 with long lines wrapped. We avoid
// directly importing the config for now since the config is still in old format
// and is expected to change to the new flat config pretty soon, and importing
// it would block the migration.
// TODO(pihsun): Import the config directly and customize upon it after the
// base config is also migrate to ESLint flat config.
const chromiumRules = {
  // Enabled checks.
  'brace-style': ['error', '1tbs'],

  // https://google.github.io/styleguide/jsguide.html#features-arrays-trailing-comma
  // https://google.github.io/styleguide/jsguide.html#features-objects-use-trailing-comma
  'comma-dangle': ['error', 'always-multiline'],

  'curly': ['error', 'multi-line', 'consistent'],
  'new-parens': 'error',
  'no-array-constructor': 'error',
  'no-console': ['error', {allow: ['info', 'warn', 'error', 'assert']}],
  'no-debugger': 'error',
  'no-extra-boolean-cast': 'error',
  'no-extra-semi': 'error',
  'no-new-wrappers': 'error',
  'no-restricted-imports': [
    'error',
    {
      paths: [
        {
          name:
            'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js',
          importNames: ['Polymer'],
          message: 'Use PolymerElement instead.',
        },
        {
          name: '//resources/polymer/v3_0/polymer/polymer_bundled.min.js',
          importNames: ['Polymer'],
          message: 'Use PolymerElement instead.',
        },
      ],
    },
  ],
  'no-restricted-properties': [
    'error',
    {
      property: '__lookupGetter__',
      message: 'Use Object.getOwnPropertyDescriptor',
    },
    {
      property: '__lookupSetter__',
      message: 'Use Object.getOwnPropertyDescriptor',
    },
    {
      property: '__defineGetter__',
      message: 'Use Object.defineProperty',
    },
    {
      property: '__defineSetter__',
      message: 'Use Object.defineProperty',
    },
    {
      object: 'cr',
      property: 'exportPath',
      message: 'Use ES modules or cr.define() instead',
    },
  ],
  'no-restricted-syntax': [
    'error',
    {
      selector:
        'CallExpression[callee.object.name=JSON][callee.property.name=parse]' +
        ' > CallExpression[callee.object.name=JSON]' +
        '[callee.property.name=stringify]',
      message: `Don't use JSON.parse(JSON.stringify(...)) to clone objects.` +
        ' Use structuredClone() instead.',
    },
    {
      // https://google.github.io/styleguide/tsguide.html#return-type-only-generics
      selector: 'TSAsExpression > CallExpression' +
        ' > MemberExpression[property.name=/^querySelector$/]',
      message: `Don't use 'querySelector(...) as Type'.` +
        ` Use the type parameter, 'querySelector<Type>(...)' instead`,
    },
    {
      // https://google.github.io/styleguide/tsguide.html#return-type-only-generics
      selector: 'TSAsExpression > TSNonNullExpression > CallExpression' +
        ' > MemberExpression[property.name=/^querySelector$/]',
      message: `Don't use 'querySelector(...)! as Type'.` +
        ` Use the type parameter, 'querySelector<Type>(...)',` +
        ' followed by an assertion instead',
    },
    {
      // https://google.github.io/styleguide/tsguide.html#return-type-only-generics
      selector: 'TSAsExpression > CallExpression' +
        ' > MemberExpression[property.name=/^querySelectorAll$/]',
      message: `Don't use 'querySelectorAll(...) as Type'.` +
        ` Use the type parameter, 'querySelectorAll<Type>(...)' instead`,
    },
    {
      // Prevent a common misuse of "!" operator.
      selector: 'TSNonNullExpression > CallExpression' +
        ' > MemberExpression[property.name=/^querySelectorAll$/]',
      message:
        'Remove unnecessary "!" non-null operator after querySelectorAll().' +
        ' It always returns a non-null result',
    },
    {
      // https://google.github.io/styleguide/jsguide.html#es-module-imports
      //  1) Matching only import URLs that have at least one '/' slash, to
      //     avoid false positives for NodeJS imports like
      //     `import fs from 'fs';`.
      //     Using '\u002F' instead of '/' as the suggested workaround for
      //     https://github.com/eslint/eslint/issues/16555
      //  2) Allowing extensions that have a length between 2-4 characters
      //     (for example js, css, json)
      selector: 'ImportDeclaration[' +
        'source.value=/^.*\\u002F.*(?<!\\.[a-z]{2}|\\.[a-z]{3}|\\.[a-z]{4})$/]',
      message: 'Disallowed extensionless import.' +
        ' Explicitly specify the extension suffix.',
    },
  ],
  'no-throw-literal': 'error',
  'no-trailing-spaces': 'error',
  'no-var': 'error',
  'prefer-const': 'error',
  'quotes': ['error', 'single', {allowTemplateLiterals: true}],
  'semi': ['error', 'always'],

  // https://google.github.io/styleguide/jsguide.html#features-one-variable-per-declaration
  'one-var': [
    'error',
    {
      'let': 'never',
      'const': 'never',
    },
  ],
};

const chromiumTsRules = {
  'no-unused-vars': 'off',
  '@typescript-eslint/no-unused-vars': [
    'error',
    {
      argsIgnorePattern: '^_',
      varsIgnorePattern: '^_',
      caughtErrorsIgnorePattern: '.*',
    },
  ],

  // https://google.github.io/styleguide/tsguide.html#array-constructor
  // Note: The rule below only partially enforces the styleguide, since it
  // it does not flag invocations of the constructor with a single
  // parameter.
  'no-array-constructor': 'off',
  '@typescript-eslint/no-array-constructor': 'error',

  // https://google.github.io/styleguide/tsguide.html#automatic-semicolon-insertion
  'semi': 'off',
  '@stylistic/semi': ['error'],

  // https://google.github.io/styleguide/tsguide.html#arrayt-type
  '@typescript-eslint/array-type': [
    'error',
    {
      'default': 'array-simple',
    },
  ],

  // https://google.github.io/styleguide/tsguide.html#type-assertions-syntax
  '@typescript-eslint/consistent-type-assertions': [
    'error',
    {
      assertionStyle: 'as',
    },
  ],

  // https://google.github.io/styleguide/tsguide.html#interfaces-vs-type-aliases
  '@typescript-eslint/consistent-type-definitions': ['error', 'interface'],

  // https://google.github.io/styleguide/tsguide.html#import-type
  '@typescript-eslint/consistent-type-imports': 'error',

  // https://google.github.io/styleguide/tsguide.html#visibility
  '@typescript-eslint/explicit-member-accessibility': [
    'error',
    {
      accessibility: 'no-public',
      overrides: {
        parameterProperties: 'off',
      },
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
          // Exclude native DOM types which are always named like
          // HTML<Foo>Element.
          'HTML[A-Za-z]{0,}Element|' +
          // Exclude native DOM interfaces.
          'UIEvent|UIEventInit|DOMError|' +
          // Exclude the deprecated WebUIListenerBehavior interface.
          'WebUIListenerBehavior)$',
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
    },
    {
      selector: 'classMethod',
      format: ['strictCamelCase'],
      modifiers: ['private'],
      trailingUnderscore: 'allow',

      // Disallow the 'Tap_' suffix, in favor of 'Click_' in event handlers.
      // Note: Unfortunately this ESLint rule does not provide a way to
      // customize the error message to better inform developers.
      custom: {
        regex: '^on[a-zA-Z0-9]+Tap$',
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

  // https://google.github.io/styleguide/tsguide.html#member-property-declarations
  '@stylistic/member-delimiter-style': [
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
      overrides: {
        'interface': {
          multiline: {
            delimiter: 'semi',
            requireLast: true,
          },
          singleline: {
            delimiter: 'semi',
            requireLast: false,
          },
        },
      },
    },
  ],

  // https://google.github.io/styleguide/tsguide.html#wrapper-types
  '@typescript-eslint/no-restricted-types': [
    'error',
    {
      types: {
        String: {
          message: 'Use string instead',
          fixWith: 'string',
        },
        Boolean: {
          message: 'Use boolean instead',
          fixWith: 'boolean',
        },
        Number: {
          message: 'Use number instead',
          fixWith: 'number',
        },
        Symbol: {
          message: 'Use symbol instead',
          fixWith: 'symbol',
        },
        BigInt: {
          message: 'Use bigint instead',
          fixWith: 'bigint',
        },
      },
    },
  ],

  // https://google.github.io/styleguide/tsguide.html#ts-ignore
  '@typescript-eslint/ban-ts-comment': ['error', {'ts-ignore': true}],
};

export default [
  {
    ignores: [
      // dist/ is generated by `cra.py bundle <board>`.
      'dist/',
      // mojom is symbolic linked by `cra.py tsc <board>` to board specific
      // generated TypeScript mojom bindings.
      'mojom',
    ],
  },
  {
    languageOptions: {
      sourceType: 'module',
      parser: tseslintParser,
    },
  },
  {
    files: ['**/*.ts'],
    languageOptions: {
      parserOptions: {
        project: './tsconfig_base.json',
        tsconfigRootDir: import.meta.dirname,
      },
    },
  },
  {
    plugins: {
      '@typescript-eslint': tseslint,
      '@stylistic': stylistic,
      jsdoc,
      cra,
    },
  },
  {
    settings: {
      jsdoc: {
        tagNamePreference: {
          returns: 'return',
          file: 'fileoverview',
        },
      },
    },
  },
  {
    name: 'eslint/recommended',
    ...eslint.configs.recommended,
  },
  {
    name: '@typescript-eslint/eslint-recommended',
    ...tseslintOverride.default('minimatch'),
  },
  {
    name: '@typescript-eslint/recommended',
    rules: tseslint.configs['recommended'].rules,
  },
  {
    name: '@typescript-eslint/stylistic',
    rules: tseslint.configs['stylistic'].rules,
  },
  // We don't use @typescript-eslint/recommended-type-checked-only since
  // there's no full type information from Lit and mojo when linting, resulting
  // in a large amount of false negative.
  {
    name: '@typescript-eslint/stylistic-type-checked-only',
    files: ['**/*.ts'],
    rules: tseslint.configs['stylistic-type-checked-only'].rules,
  },
  {
    name: 'chromium/base',
    rules: chromiumRules,
  },
  {
    name: 'chromium/ts',
    files: ['**/*.ts'],
    rules: chromiumTsRules,
  },
  // Generally, the rules should be compatible to both bundled and the newest,
  // stable eslint, so it's easier to upgrade and develop without the full
  // Chromium tree.
  // These are the rules that are based on the chromiumRules and are more
  // strict. We move these to a separate set to make it easier to see which
  // rules are intended to override Chromium rules.
  {
    name: 'cra/chromium-override',
    rules: {
      // Upgrade several deprecated rules to the @stylistic counterpart.
      ...Object.fromEntries(
        [
          'brace-style',
          'comma-dangle',
          'new-parens',
          'no-extra-semi',
          'no-trailing-spaces',
          'quotes',
          'semi',
        ].flatMap((r) => {
          return [
            [r, 'off'],
            [`@stylistic/${r}`, chromiumRules[r]],
          ];
        }),
      ),

      // Using "as" type assertion should be rare and as a last resort if it's
      // really too complicated to put the constraint in type system, and it's
      // not easy to do a runtime assertion (assertInstanceof) either.
      //
      // If it's the case, please have a eslint-disable-next-line to disable the
      // lint together with some comment explaining why the assertion is safe.
      //
      // See also:
      // go/tsstyle#type-and-non-nullability-assertions
      // go/tsstyle#type-assertions-syntax
      // go/tsstyle#type-assertions-and-object-literals
      '@typescript-eslint/consistent-type-assertions': [
        'error',
        {
          assertionStyle: 'never',
        },
      ],

      'no-restricted-syntax': [
        ...chromiumRules['no-restricted-syntax'],
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
          message:
            'Private fields are not allowed. (go/tsstyle#private-fields)',
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
        ...chromiumTsRules['@typescript-eslint/naming-convention'],
        {
          selector: 'default',
          format: ['strictCamelCase'],
        },
        // Allows leading underscore only for unused parameters.
        {
          selector: 'parameter',
          format: ['strictCamelCase'],
        },
        {
          selector: 'parameter',
          modifiers: ['unused'],
          format: ['strictCamelCase'],
          leadingUnderscore: 'allow',
        },
        {
          selector: 'variable',
          format: ['strictCamelCase', 'UPPER_CASE'],
        },
        // This is a copy from the Chromium default TypeScript rules, but with
        // more strict requirement from camelCase to strictCamelCase.
        {
          selector: 'classProperty',
          format: ['strictCamelCase'],
          modifiers: ['public'],
        },
        {
          selector: 'classProperty',
          format: ['strictCamelCase'],
          modifiers: ['private'],
          trailingUnderscore: 'allow',
        },
        {
          selector: 'parameter',
          format: ['strictCamelCase'],
          leadingUnderscore: 'allow',
        },
        {
          selector: 'function',
          format: ['strictCamelCase'],
        },
        {
          // This is used in all lit file. Since the HTMLElementTagNameMap name
          // is not defined by us, ignore naming-convention lint check on it.
          selector: 'interface',
          format: [],
          filter: {
            regex: '^HTMLElementTagNameMap$',
            match: true,
          },
        },
        {
          // This is for the tag name declaration in all lit file. Added here
          // since it's cumbersome to have eslint disable in all lit files, and
          // it feels quite hard to accidentally triggers this rule.
          selector: 'typeProperty',
          format: [],
          filter: {
            regex: '^[a-z-]*$',
            match: true,
          },
        },
        {
          // This is for having CSS class names in the classMap arguments. Since
          // CSS classes use -, it doesn't follow the typical strictCamelCase
          // object literal property rule. Since it's not easy to accidentally
          // have those by mistake, and there's no good way of telling whether
          // an object literal property is used as CSS class name, we allow all
          // object literal property to match /[a-z-]*/.
          selector: 'objectLiteralProperty',
          format: [],
          filter: {
            regex: '^[a-z-]*$',
            match: true,
          },
        },
      ],

      // go/tsstyle#import-export-type
      '@typescript-eslint/consistent-type-imports': [
        'error',
        {
          // TODO: b/369962714 - Chromium wide default is always write "import
          // type", but that cause clang-format to crash under our current
          // .clang-format, and without those settings all import are on the
          // same line which is really unreadable for longer imports.
          // We should remove this lint customization and align with Chromium
          // settings when the clang-format crash is fixed.
          prefer: 'no-type-imports',
        },
      ],
    },
  },
  {
    name: 'cra/extra',
    rules: {
      // go/tsstyle#switch-statements
      'default-case': 'error',

      'eqeqeq': 'error',
      'new-cap': 'error',
      'no-caller': 'error',

      'no-constant-condition': ['error', {checkLoops: false}],
      'no-extend-native': 'error',
      'no-extra-bind': 'error',
      'no-multi-str': 'error',
      'no-new-native-nonconstructor': 'error',
      'no-object-constructor': 'error',
      'prefer-promise-reject-errors': 'error',

      // @typescript-eslint rules

      // go/tsstyle#return-types
      '@typescript-eslint/explicit-module-boundary-types': 'error',

      '@typescript-eslint/no-invalid-this': 'error',
      '@typescript-eslint/no-non-null-assertion': 'error',

      // TODO(pihsun): This rule is deprecated, remove this after considering if
      // it's beneficial to add eslint-plugin-perfectionist.
      '@typescript-eslint/sort-type-constituents': 'error',
    },
  },
  {
    name: 'cra/jsdoc',
    rules: {
      'jsdoc/check-access': 'error',
      'jsdoc/check-alignment': 'error',
      'jsdoc/check-param-names': 'error',
      'jsdoc/check-property-names': 'error',
      'jsdoc/check-tag-names': 'error',
      'jsdoc/check-types': 'error',
      'jsdoc/check-values': 'error',
      'jsdoc/empty-tags': 'error',
      'jsdoc/implements-on-classes': 'error',
      'jsdoc/multiline-blocks': [
        'error',
        {
          noSingleLineBlocks: true,
        },
      ],
      'jsdoc/no-bad-blocks': [
        'error',
        {
          // The first four are default values, and the last one is added since
          // the lint name is too long and the eslint-disable-next-line is
          // frequently line wrapped, which cause jsdoc/no-bad-blocks to think
          // that it should be a docstring.
          ignore: [
            'ts-check',
            'ts-expect-error',
            'ts-ignore',
            'ts-nocheck',
            'typescript-eslint/consistent-type-assertions',
          ],
        },
      ],
      'jsdoc/no-defaults': 'error',
      'jsdoc/no-multi-asterisks': [
        'error',
        {
          allowWhitespace: true,
        },
      ],
      'jsdoc/no-undefined-types': 'error',
      'jsdoc/require-asterisk-prefix': 'error',
      'jsdoc/require-description-complete-sentence': [
        'error',
        {
          abbreviations: ['e.g.'],
        },
      ],
      'jsdoc/require-jsdoc': [
        'error',
        {
          publicOnly: true,
        },
      ],
      'jsdoc/require-param': 'off',
      'jsdoc/require-param-description': 'error',
      'jsdoc/require-param-name': 'error',
      'jsdoc/require-param-type': 'off',
      'jsdoc/require-property': 'error',
      'jsdoc/require-property-description': 'error',
      'jsdoc/require-property-name': 'error',
      'jsdoc/require-property-type': 'error',
      'jsdoc/require-returns': 'off',
      'jsdoc/require-returns-check': 'error',
      'jsdoc/require-returns-description': 'error',
      'jsdoc/require-returns-type': 'off',
      'jsdoc/require-yields': 'off',
      'jsdoc/require-yields-check': 'error',
      'jsdoc/tag-lines': ['error', 'never', {startLines: 1}],
      'jsdoc/valid-types': 'error',
    },
  },
  {
    name: 'cra/stylistic',
    rules: {
      '@stylistic/arrow-parens': 'error',
      '@stylistic/eol-last': 'error',
      '@stylistic/linebreak-style': 'error',
      '@stylistic/lines-between-class-members': 'error',
      '@stylistic/max-len': [
        'error',
        {
          code: 80,
          tabWidth: 2,
          ignoreUrls: true,
        },
      ],
      '@stylistic/no-multi-spaces': ['error', {ignoreEOLComments: true}],
      '@stylistic/no-multiple-empty-lines': ['error', {max: 2}],
      '@stylistic/operator-linebreak': ['error', 'after'],
      '@stylistic/padded-blocks': ['error', 'never'],
      '@stylistic/quote-props': [
        'error',
        'consistent-as-needed',
        {keywords: true},
      ],
      '@stylistic/spaced-comment': ['error', 'always'],
    },
  },
  {
    name: 'cra/custom',
    rules: {
      'cra/parameter-comment-format': 'error',
      'cra/generic-parameter-on-declaration-type': 'error',
      'cra/todo-format': 'error',
    },
  },
  {
    name: 'cra/ts',
    files: ['**/*.ts'],
    // TODO(pihsun): extends @typescript-eslint/recommended-type-checked when
    // we can really have the board dependent types to be correct. Currently
    // there's a lot of false negative because of unrecognized types treated
    // as `any`. We should be able to refer most used types of lit / mwc from
    // source instead of from gen folder, and exclude mojo related things from
    // the checks.
    rules: {
      // go/tsstyle#omit-comments-that-are-redundant-with-typescript
      'jsdoc/check-tag-names': ['error', {typed: true}],
      'jsdoc/no-types': 'error',

      // go/tsstyle#optimization-compatibility-for-property-access
      '@typescript-eslint/dot-notation': 'error',

      '@typescript-eslint/prefer-nullish-coalescing': 'error',
      '@typescript-eslint/prefer-optional-chain': 'error',

      // go/tsstyle#use-readonly
      '@typescript-eslint/prefer-readonly': 'error',

      '@typescript-eslint/require-array-sort-compare': 'error',
      '@typescript-eslint/return-await': 'error',
      '@typescript-eslint/strict-boolean-expressions': [
        'error',
        {
          allowString: false,
          allowNumber: false,
          allowNullableObject: false,
          // `any` is allowed here since our .eslintrc doesn't use the full
          // tsconfig.json (contains reference to board specific files), which
          // cause this rule to have some false negative on unrecognized
          // types.
          // TODO(pihsun): Change the lint action to be board dependent if we
          // can find a way to keep running it on presubmit check.
          allowAny: true,
        },
      ],

      // Prevent floating promises, since promises that are not awaited
      // usually indicates improper sequencing that might cause race, and if
      // the promise is rejected, the error is only logged by unhandled
      // promise rejection, and not propagated to caller.
      //
      // There are several potential ways to fix the lint error if you
      // encounter this:
      // * If the caller should wait for the promise, make the caller async
      //   and await the promise.
      // * If the caller doesn't want to wait for the promise, and the promise
      //   is some kind of "job" that should be run independently but multiple
      //   jobs shouldn't be run at the same time, consider using
      //   AsyncJobQueue in async_job_queue.ts.
      // * As a last resort, add "void" before the promise to suppress the
      //   lint, ideally with a comment explaining why that is needed, check
      //   that there won't be issue if multiple of those promises got created
      //   at the same time, and check that error handling with unhandled
      //   promise rejection is sufficient.
      '@typescript-eslint/no-floating-promises': 'error',
      '@typescript-eslint/require-await': 'error',
      '@typescript-eslint/await-thenable': 'error',
      '@typescript-eslint/no-meaningless-void-operator': 'error',
      '@typescript-eslint/no-misused-promises': 'error',
    },
  },
  {
    name: 'cra/dev-exclude',
    files: ['platforms/dev/*.ts'],
    rules: {
      // Many dev mocks just log a message to console.
      'no-console': 'off',
      // Many dev mocks are empty functions now.
      '@typescript-eslint/no-empty-function': 'off',
      // Many dev mocks overrides the parent async method but doesn't really
      // need to await.
      '@typescript-eslint/require-await': 'off',
    },
  },
];
