// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// By injecting this into Google Docs pages, ChromeVox can inform
// docs to turn on A11y mode with braille.
const event = document.createEvent('UIEvents');
event.initEvent('chromeVoxLoaded', true, false);
document.dispatchEvent(event);
