// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export function getBase64EncodedSrcForPng(pngBytes: number[]): string {
  const image = btoa(String.fromCharCode(...pngBytes));
  return 'data:image/png;base64,' + image;
}
