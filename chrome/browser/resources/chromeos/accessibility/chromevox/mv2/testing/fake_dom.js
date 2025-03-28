// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Maps some objects/functions within an ordinary web renderer to a v8 one.
window = {};
window.setTimeout = () => {};
globalThis = window;
goog = {};
goog.provide = () => {};
goog.require = () => {};
goog.addDependency = () => {};
goog.isDef = val => val !== undefined;
