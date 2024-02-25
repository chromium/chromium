// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

type PiexLoadPromise = Promise<((buffer: ArrayBuffer) => Promise<File>)>;
let _piexLoadPromise: PiexLoadPromise|null = null;

/**
 * Loads PIEX, the "Preview Image Extractor", via wasm.
 */
export async function loadPiex(): PiexLoadPromise {
  async function startLoad() {
    function loadJs(path : string, module : boolean) {
      return new Promise((resolve, reject) => {
        const script = document.createElement('script') as HTMLScriptElement;
        script.onload = resolve;
        script.onerror = reject;
        if (module) {
          script.type = 'module';
        }
        script.src = path;
        if (document.head) {
          document.head.appendChild(script);
        }
      });
    }
    await loadJs('piex/piex.js.wasm', false);
    await loadJs('piex_module.js', true);
    return window.extractFromRawImageBuffer;
  }
  if (!_piexLoadPromise) {
    _piexLoadPromise = startLoad();
  }
  return _piexLoadPromise!;
}
