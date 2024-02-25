// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ChromeVox compiled script (non-ES6 modules) packed into one file.
importScripts('chromeVoxChromeBackgroundScript.js');

// ChromeVox ES6 modules. Non-ES6 modules cannot depend on these modules.
import 'background/es6_loader.js';

// Third party - Speech Rule Engine.
importScripts('third_party/sre/sre_browser.js');
