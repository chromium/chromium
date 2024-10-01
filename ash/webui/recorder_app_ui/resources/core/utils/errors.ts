// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Code location of stack frame.
 */
export interface StackFrame {
  fileName: string;
  funcName: string;
  lineNo: number;
  colNo: number;
}

/**
 * Parse the information from the stack trace extracted from an error event.
 */
export function parseTopFrameInfo(stackTrace: string): StackFrame {
  const regex = /at (\[?\w+\]? |)\(?(.+):(\d+):(\d+)/;
  const match = regex.exec(stackTrace);
  return {
    funcName: match?.[1]?.trim() ?? '',
    fileName: match?.[2] ?? '',
    lineNo: Number(match?.[3] ?? -1),
    colNo: Number(match?.[4] ?? -1),
  };
}
