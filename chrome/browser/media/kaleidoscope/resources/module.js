// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export async function loadKaleidoscopeModule() {
  return {
    element: document.createElement('div'),
    title: loadTimeData.getString('modulesKaleidoscopeTitle'),
  };
}
