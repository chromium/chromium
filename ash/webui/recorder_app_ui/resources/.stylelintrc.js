// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const config = {
  'extends': 'stylelint-config-standard',
  'rules': {
    'color-function-notation': ['modern', {ignore: ['with-var-inside']}],
    'custom-property-pattern': [
      '^((([a-z][a-z0-9]*)(-[a-z0-9]+)*)|(cros-[a-z0-9_-]+))$',
      {
        message: (name) =>
          `Expected custom property name "${name}" to be kebab-case or cros-*`,
      },
    ],
    'function-no-unknown': ['error', {ignoreFunctions: 'inset-area'}],
    'no-descending-specificity': 'off',
    'property-no-unknown': ['error', {ignoreProperties: 'position-anchor'}],
    'property-no-vendor-prefix': 'off',
  },
  'overrides': [
    {
      files: ['*.ts', '**/*.ts'],
      customSyntax: 'postcss-lit',
    },
  ],
};

/* global module */
module.exports = config;
