// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Exports loadTimeData while guaranteeing that project-specific
 * strings are imported. This should be used instead of importing loadTimeData
 * directly.
 */

import '/strings.m.js';

export {loadTimeData} from 'chrome://resources/js/load_time_data.js';
