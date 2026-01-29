// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Convert legacy module names (i.e. module uid) to a format that can
// be captured in metrics.
//
// Previous module names were created in the format `NNN-module-name`.
// These module names cannot be recorded in metrics as-is, due to the
// hyphens. They must be switched to a PascalCase format. New module
// names will not follow this format, therefore will be ignored if they
// don't contain a hyphen.
//
// Exported for testing purposes only.
export function formatModuleName(moduleName: string) {
  if (!moduleName.includes('-')) {
    return moduleName;
  }
  // Remove the 3 numbers at the beginning of the name (`NNN-`)
  const withoutPrefix = moduleName.replace(/^\d{3}-/, '');
  return kebabCaseToCamelCase(withoutPrefix);
}

// Replace first letter with its uppercase equivalent.
function uppercaseFirstLetter(word: string) {
  return word.replace(/^\w/, firstLetter => firstLetter.toUpperCase());
}

// Convert kebab-case string (e.g. my-module-name) to PascalCase (e.g.
// MyModuleName).
function kebabCaseToCamelCase(input: string) {
  return input
      // Split on hyphen to remove it.
      .split('-')
      // Uppercase first letter of each word.
      .map(uppercaseFirstLetter)
      // Join back into contiguous string.
      .join('');
}
