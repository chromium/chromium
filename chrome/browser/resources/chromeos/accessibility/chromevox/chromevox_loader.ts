// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Closure scripts. Each call to importScripts() depends on the scripts in the previous calls.
// Can be found in //third_party/chromevox/third_party/closure_library/closure/goog/.
importScripts('../closure/base.js');
importScripts(
    '../closure/debug/error.js','../closure/dom/nodetype.js', '../closure/i18n/ordinalrules.js',
    '../closure/i18n/pluralrules.js', '../closure/string/string.js');
importScripts('../closure/asserts/asserts.js');
importScripts('../closure/i18n/messageformat.js');
importScripts('common/closure_loader.js');

// ChromeVox ES6 modules. Non-ES6 modules cannot depend on these modules.
import 'background/es6_loader.js';

// Third party - Speech Rule Engine.
importScripts('third_party/sre/sre_browser.js');
