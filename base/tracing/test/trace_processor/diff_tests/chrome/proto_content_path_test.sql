-- Copyright 2023 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

SELECT content.total_size,
  frame.field_type, frame.field_name,
  frame.parent_id,
  EXTRACT_ARG(frame.arg_set_id, 'event.category') AS event_category,
  EXTRACT_ARG(frame.arg_set_id, 'event.name') AS event_name
FROM experimental_proto_path AS frame JOIN experimental_proto_content AS content ON content.path_id = frame.id
ORDER BY total_size DESC, path
LIMIT 10;
