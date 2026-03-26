// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export function buildSystemInstruction(
    persona: string, url: string, title?: string,
    pageContent?: string): string {
  let instruction = `${persona}

## Current Page
[${title ?? '<N/A>'}](${url})`;

  if (pageContent) {
    instruction += `

## Current Page Content

${pageContent}`;
  }

  return instruction;
}
