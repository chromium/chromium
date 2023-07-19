-- Copyright 2023 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

-- Returns hardware class of the device, often use to find device brand
-- and model.
-- @ret STRING Hardware class name.
CREATE PERFETTO FUNCTION CHROME_HARDWARE_CLASS()
RETURNS STRING AS
SELECT
  str_value
  FROM metadata
WHERE name = "cr-hardware-class";