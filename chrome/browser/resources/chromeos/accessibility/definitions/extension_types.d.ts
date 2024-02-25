// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Definitions for chrome.extensionTypes API
 * Generated from: extensions/common/api/extension_types.json
 * run `tools/json_schema_compiler/compiler.py
 * extensions/common/api/extension_types.json -g ts_definitions` to regenerate.
 */



declare namespace chrome {
  export namespace extensionTypes {

    export enum ImageFormat {
      JPEG = 'jpeg',
      PNG = 'png',
    }

    export interface ImageDetails {
      format?: ImageFormat;
      quality?: number;
    }

    export enum RunAt {
      DOCUMENT_START = 'document_start',
      DOCUMENT_END = 'document_end',
      DOCUMENT_IDLE = 'document_idle',
    }

    // eslint-disable-next-line @typescript-eslint/naming-convention
    export enum CSSOrigin {
      AUTHOR = 'author',
      USER = 'user',
    }

    export interface InjectDetails {
      code?: string;
      file?: string;
      allFrames?: boolean;
      frameId?: number;
      matchAboutBlank?: boolean;
      runAt?: RunAt;
      cssOrigin?: CSSOrigin;
    }

    export interface DeleteInjectionDetails {
      code?: string;
      file?: string;
      allFrames?: boolean;
      frameId?: number;
      matchAboutBlank?: boolean;
      cssOrigin?: CSSOrigin;
    }

    export enum FrameType {
      OUTERMOST_FRAME = 'outermost_frame',
      FENCED_FRAME = 'fenced_frame',
      SUB_FRAME = 'sub_frame',
    }

    export enum DocumentLifecycle {
      PRERENDER = 'prerender',
      ACTIVE = 'active',
      CACHED = 'cached',
      PENDING_DELETION = 'pending_deletion',
    }

    export enum ExecutionWorld {
      ISOLATED = 'ISOLATED',
      MAIN = 'MAIN',
      USER_SCRIPT = 'USER_SCRIPT',
    }

  }
}
