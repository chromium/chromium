// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

if (new URLSearchParams(window.location.search).get('cs') === '1') {
  document.documentElement.toggleAttribute('dark-mode', true);
}
