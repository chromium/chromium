// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This file should only be included once. It will generate an assert error if
 * it's included more than once, which can happen when an include is misspelled.
 */

cr.exportPath('welcome');
assert(
    !welcome.defaultResourceLoaded,
    'welcome.js run twice. You probably have an invalid import.');
/** Global defined when the main welcome script runs. */
welcome.defaultResourceLoaded = true;
