// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The minimum transcript word length for title generation and summarization.
export const MIN_WORD_LENGTH = 150;

// The input token limit is 2048 and 3 words roughly equals to 4
// tokens. Having a conservative limit here and leaving some room for the
// template.
// TODO: b/336477498 - Get the token limit from server and accurately count the
// token size with the same tokenizer.
// TODO: b/358233121 - Make this configurable for different models.
export const MAX_WORD_LENGTH = Math.floor(((2048 - 100) / 4) * 3);
