-- Copyright 2023 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

SELECT RUN_METRIC('experimental/chrome_dropped_frames.sql');

SELECT * FROM dropped_frames_with_process_info;
