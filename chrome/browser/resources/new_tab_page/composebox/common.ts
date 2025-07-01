// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface ComposeboxFile {
  // TODO(crbug.com/427994425): Keep uuid until we get base::Unguessable token
  //   through a callback.
  uuid: string;
  name: string;
  objectUrl: string|null;
  type: string;
}
