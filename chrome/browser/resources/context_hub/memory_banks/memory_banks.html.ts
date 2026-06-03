// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {MemoryBanksElement} from './memory_banks.js';

export function getHtml(this: MemoryBanksElement) {
  return html`
    <main id="memory-banks-view">
        <section>
            <h1>Memory banks</h1>
        </section>
    </main>
  `;
}
