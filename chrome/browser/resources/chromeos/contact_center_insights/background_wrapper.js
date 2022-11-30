// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Registration fails for the service worker when there are runtime errors in
// the background worker script, so we use this wrapper to catch all runtime
// errors so they can be handled and reported gracefully.
try {
  importScripts('background.js');
} catch (error) {
  console.error(error);
}
