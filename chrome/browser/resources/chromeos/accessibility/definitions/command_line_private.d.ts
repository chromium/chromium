// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Definitions for chrome.commandLinePrivate API
 * Generated from: chrome/common/extensions/api/command_line_private.json
 * run `tools/json_schema_compiler/compiler.py
 * chrome/common/extensions/api/command_line_private.json -g ts_definitions` to
 * regenerate.
 */



declare namespace chrome {
  export namespace commandLinePrivate {

    export function hasSwitch(
        name: string, callback: (result: boolean) => void): void;

  }
}
