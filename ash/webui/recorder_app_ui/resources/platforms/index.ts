// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is replaced in dev.py when in local dev, and should be the only
// place that directly import the PlatformHandler from subdirectory.
//
// Note that most user should import core/platform_handler.ts instead, and this
// should only be directly imported by init.ts, or other files that need to
// call static methods on the class (currently only core/i18n.ts).
export {PlatformHandler} from './swa/handler.js';
