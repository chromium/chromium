// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export function buildSystemInstruction(
    persona: string, title: string, url: string): string {
  return `${persona}

## Current Page
[${title}](${url})

You are a helpful assistant in a Chrome overlay. Keep responses brief and
conversational.`;
}
