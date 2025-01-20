-- Copyright 2024 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

INCLUDE PERFETTO MODULE slices.with_context;

-- This module defines tables with information about Android input pipeline
-- steps. The trace needs to be recorded with the 'view' atrace category.

-- On Android, input goes through the following path before getting to Chrome:
--  * InputReader thread (part of Android system_server)
--  * InputDispatcher thread (part of Android system_server)
--  * Browser Main thread (Chromium/Chrome)

-- In traces, each of these three steps have slices which are implicitly linked
-- together by an input id (part of slice name) assigned by the Android system.

-- The following queries correlate the three steps mentioned above
-- with the rest of the `LatencyInfo.Flow` pipeline.

-- InputReader is the first step in the input pipeline.
-- It is responsible for reading the input events from the system_server
-- process and sending them to the InputDispatcher (which then sends them
-- to the browser process).

CREATE PERFETTO TABLE _chrome_android_motion_input_reader_step(
-- Input reader step timestamp.
  ts TIMESTAMP,
  -- Input reader step duration.
  dur DURATION,
  -- Input reader step slice id.
  id LONG,
  -- Input id.
  android_input_id STRING,
   -- Input reader step utid.
  utid LONG
)
AS
SELECT
  ts,
  dur,
  id,
  -- Get the substring that starts with 'id=', remove the 'id=' and remove the trailing ')'.
  -- 'id=0x344bb0f9)' ->  '0x344bb0f9'
  TRIM(
    SUBSTR(
      SUBSTR(name, INSTR(name, 'id='))
    , 4),
  ')')
  AS android_input_id,
  utid
FROM thread_slice AS slice
WHERE
  name GLOB 'UnwantedInteractionBlocker::notifyMotion*';

-- InputDispatcher is the second step in the input pipeline.
-- It is responsible for dispatching the input events to the browser process.
CREATE PERFETTO TABLE _chrome_android_motion_input_dispatcher_step(
  -- Input dispatcher step timestamp.
  ts TIMESTAMP,
  -- Input dispatcher step duration.
  dur DURATION,
  -- Input dispatcher step slice id.
  id LONG,
  -- Input id.
  android_input_id STRING,
   -- Input dispatcher step utid.
  utid LONG
)
AS
SELECT
  ts,
  dur,
  id,
  TRIM(
  SUBSTR(
  SUBSTR(name, INSTR(name, 'id='))
  , 4), ')')
  AS android_input_id,
  utid
FROM thread_slice AS slice
WHERE
  name GLOB 'prepareDispatchCycleLocked*chrome*';

-- DeliverInputEvent is the third step in the input pipeline.
-- It is responsible for routing the input events within browser process.
CREATE PERFETTO TABLE chrome_deliver_android_input_event(
  -- Timestamp.
  ts TIMESTAMP,
  -- Touch move processing duration.
  dur DURATION,
  -- Utid.
  utid LONG,
  -- Input id (assigned by the system, used by InputReader and InputDispatcher)
  android_input_id STRING
) AS
SELECT
  slice.ts,
  slice.dur,
  slice.utid,
  SUBSTR(SUBSTR(name, INSTR(name, 'id=')), 4) AS android_input_id
FROM
  thread_slice AS slice
WHERE
  slice.name GLOB 'deliverInputEvent*';

-- Collects information about input reader, input dispatcher and
-- DeliverInputEvent steps for the given Android input id.
CREATE PERFETTO TABLE chrome_android_input(
  -- Input id.
  android_input_id STRING,
  -- Input reader step start timestamp.
  input_reader_processing_start_ts TIMESTAMP,
  -- Input reader step end timestamp.
  input_reader_processing_end_ts TIMESTAMP,
  -- Input reader step utid.
  input_reader_utid LONG,
  -- Input dispatcher step start timestamp.
  input_dispatcher_processing_start_ts TIMESTAMP,
  -- Input dispatcher step end timestamp.
  input_dispatcher_processing_end_ts TIMESTAMP,
  -- Input dispatcher step utid.
  input_dispatcher_utid LONG,
  -- DeliverInputEvent step start timestamp.
  deliver_input_event_start_ts TIMESTAMP,
  -- DeliverInputEvent step end timestamp.
  deliver_input_event_end_ts TIMESTAMP,
  -- DeliverInputEvent step utid.
  deliver_input_event_utid LONG
) AS
SELECT
  _chrome_android_motion_input_reader_step.android_input_id,
  _chrome_android_motion_input_reader_step.ts AS input_reader_processing_start_ts,
  _chrome_android_motion_input_reader_step.ts +
  _chrome_android_motion_input_reader_step.dur AS input_reader_processing_end_ts,
  _chrome_android_motion_input_reader_step.utid AS input_reader_utid,
  _chrome_android_motion_input_dispatcher_step.ts AS input_dispatcher_processing_start_ts,
  _chrome_android_motion_input_dispatcher_step.ts +
  _chrome_android_motion_input_dispatcher_step.dur AS input_dispatcher_processing_end_ts,
  _chrome_android_motion_input_dispatcher_step.utid AS input_dispatcher_utid,
  chrome_deliver_android_input_event.ts AS deliver_input_event_start_ts,
  chrome_deliver_android_input_event.ts +
  chrome_deliver_android_input_event.dur AS deliver_input_event_end_ts,
  chrome_deliver_android_input_event.utid AS deliver_input_event_utid
FROM
  _chrome_android_motion_input_reader_step
LEFT JOIN
  _chrome_android_motion_input_dispatcher_step USING(android_input_id)
LEFT JOIN
  chrome_deliver_android_input_event USING(android_input_id)
