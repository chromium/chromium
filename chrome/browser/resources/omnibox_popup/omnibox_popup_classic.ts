// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Entry point for the classic (dropdown-only) WebUI omnibox popup, i.e.
// omnibox_popup.html. Only pulls in what the classic popup renders, so that
// every browser window doesn't pay for parsing and defining the AIM composebox
// and full WebUI omnibox elements that live in the omnibox_popup.js bundle.
import './profile_load_time_data.js';
import './app.js';
