import typescriptEslint from '../../../../../third_party/node/node_modules/@typescript-eslint/eslint-plugin/dist/index.js';
import tsParser from '../../../../../third_party/node/node_modules/@typescript-eslint/parser/dist/index.js';

export default [
  {
    files: ['**/*.ts'],
    plugins: {
      '@typescript-eslint': typescriptEslint,
    },
    languageOptions: {
      parser: tsParser,
    },
    rules: {
      '@typescript-eslint/naming-convention': [
        'error',
        {
          selector: 'property',
          format: ['camelCase'],
        },
      ],
      'no-restricted-syntax': [
        'error',
        {
          selector:
              ':not(TSModuleBlock) > ' +
              'ExportNamedDeclaration > ' +
              'TSInterfaceDeclaration:not([declare=true])' +
              ':not([id.name="PrivateTypes"])' +
              ':not([id.name="ClosedEnums"])',
          message: 'Exported interfaces must be declared with `declare`.',
        },
      ],
    },
  },
];
