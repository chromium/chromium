// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Converts Float32 audio data to Int16 PCM and encodes to Base64.
 */
export function encodeFloat32ToPcmBase64(float32Data: Float32Array): string {
  const int16Data = new Int16Array(float32Data.length);
  for (let i = 0; i < float32Data.length; i++) {
    int16Data[i] = Math.max(-1, Math.min(1, float32Data[i]!)) * 32767;
  }

  const buffer = int16Data.buffer;
  const uint8View = new Uint8Array(buffer);
  let binary = '';
  for (let i = 0; i < uint8View.length; i++) {
    binary += String.fromCharCode(uint8View[i]!);
  }
  return btoa(binary);
}
