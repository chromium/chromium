-- Copyright 2023 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

SELECT path, SUM(total_size) as total_size
FROM experimental_proto_content as content JOIN experimental_proto_path as frame ON content.path_id = frame.id
GROUP BY path
ORDER BY total_size DESC, path
LIMIT 10;
