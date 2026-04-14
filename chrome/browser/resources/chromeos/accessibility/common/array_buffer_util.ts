// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export namespace ArrayBufferUtil {
  // Compression format to use. Only `gzip` and `deflate` are supported.
  const compressionFormat = 'gzip';

  async function compress(buffer: ArrayBuffer): Promise<ArrayBuffer> {
    const cs = new CompressionStream(compressionFormat);
    const readableStream = new ReadableStream({
      start(controller) {
        controller.enqueue(new Uint8Array(buffer));
        controller.close();
      }
    });

    const stream = readableStream.pipeThrough(cs);
    const response = new Response(stream);
    const blob = await response.blob();
    return await blob.arrayBuffer();
  }

  async function decompress(buffer: ArrayBuffer): Promise<ArrayBuffer> {
    const ds = new DecompressionStream(compressionFormat);
    const readableStream = new ReadableStream({
      start(controller) {
        controller.enqueue(new Uint8Array(buffer));
        controller.close();
      }
    });

    const stream = readableStream.pipeThrough(ds);
    const response = new Response(stream);
    const blob = await response.blob();
    return await blob.arrayBuffer();
  }

  // Function to convert ArrayBuffer to Base64 string
  export async function arrayBufferToBase64(inBuffer: ArrayBuffer):
      Promise<string> {
    const buffer = await compress(inBuffer);
    let binary = '';
    const bytes = new Uint8Array(buffer);
    const len = bytes.byteLength;
    for (let i = 0; i < len; i++) {
      binary += String.fromCharCode(bytes[i]);
    }
    return btoa(binary);  // btoa is a global function for Base64 encoding
  }

  // Function to convert Base64 string back to ArrayBuffer
  export async function base64ToArrayBuffer(base64: string):
      Promise<ArrayBuffer> {
    const binaryString =
        atob(base64);  // atob is a global function for Base64 decoding
    const len = binaryString.length;
    const bytes = new Uint8Array(len);
    for (let i = 0; i < len; i++) {
      bytes[i] = binaryString.charCodeAt(i);
    }
    return await decompress(bytes.buffer);
  }
}
