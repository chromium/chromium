// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const config = {
  extends: 'stylelint-config-standard',
  rules: {
    'custom-property-pattern': [
      '^((([a-z][a-z0-9]*)(-[a-z0-9]+)*)|(cros-[a-z0-9_-]+))$',
      {
        message: (name) => `Expected custom property name "${
            name}" to be kebab-case or cros-*`,
      },
    ],
    'color-function-notation': ['modern', {ignore: ['with-var-inside']}],
    'property-no-vendor-prefix': 'off',
    'property-no-unknown': ['error', {ignoreProperties: 'position-anchor'}],
    'function-no-unknown': ['error', {ignoreFunctions: 'inset-area'}],
  },
  overrides: [
    {
      files: ['*.ts', '**/*.ts'],
      customSyntax: 'postcss-lit',
    },
  ],
};

module.exports = config;
