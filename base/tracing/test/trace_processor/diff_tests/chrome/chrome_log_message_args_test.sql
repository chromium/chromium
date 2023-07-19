-- Copyright 2023 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

SELECT
  EXTRACT_ARG(s.arg_set_id, 'track_event.log_message') AS log_message,
  EXTRACT_ARG(s.arg_set_id, 'track_event.log_message.function_name') AS function_name,
  EXTRACT_ARG(s.arg_set_id, 'track_event.log_message.file_name') AS file_name,
  EXTRACT_ARG(s.arg_set_id, 'track_event.log_message.line_number') AS line_number
FROM
  slice s;
