// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * CrOS Settings SWA specific ESLint rules.
 */

module.exports = {
  // Disable clang-format because it produces odd formatting for these rules.
  // clang-format off
  rules: {
    /**
     * https://google.github.io/styleguide/tsguide.html#return-types
     * The Google TS style guide makes no formal rule on enforcing explicit
     * return types. However, explicit return types have clear advantages in
     * both readability and maintainability.
     */
    '@typescript-eslint/explicit-function-return-type': [
      'error',
      {
        // Function expressions are exempt.
        allowExpressions: true,
        // Avoid checking Polymer static getter methods.
        allowedNames: ['is', 'template', 'properties', 'observers'],
      },
    ],
    'quote-props': ['error', 'consistent-as-needed'],
  },
  // clang-format on
};
