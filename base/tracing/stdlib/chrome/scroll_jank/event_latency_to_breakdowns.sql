-- Copyright 2023 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.
--
-- Creates metric with info about breakdowns and jank for GestureScrollBegin and GestureScrollUpdate.

-- Select EventLatency events.
CREATE VIEW internal_event_latency
AS
SELECT
  *,
  EXTRACT_ARG(arg_set_id, "event_latency.event_type") AS event_type
FROM slice
WHERE
  name = "EventLatency";

-- Select breakdowns related to EventLatencies.
--
-- @column slice_id                  ID of the slice.
-- @column name                      Slice name.
-- @column dur                       Slice duration.
-- @column track_id                  Track ID of the slice.
-- @column ts                        Timestamp of the slice.
-- @column event_latency_id          ID of the associated EventLatency slice.
-- @column event_latency_track_id    Track ID of the associated EventLatency.
-- @column event_latency_ts          Timestamp of the associated EventLatency.
-- @column event_latency_dur         Duration of the associated EventLatency.
-- @column event_type                Event type of the associated EventLatency.
CREATE VIEW chrome_event_latency_breakdowns
AS
SELECT
  slice.id AS slice_id,
  slice.name AS name,
  slice.dur AS dur,
  slice.track_id AS track_id,
  slice.ts AS ts,
  internal_event_latency.slice_id AS event_latency_id,
  internal_event_latency.track_id AS event_latency_track_id,
  internal_event_latency.ts AS event_latency_ts,
  internal_event_latency.dur AS event_latency_dur,
  internal_event_latency.event_type AS event_type
FROM slice JOIN internal_event_latency
  ON slice.parent_id = internal_event_latency.slice_id;

-- The function takes a breakdown name and checks if the breakdown name is known or not.
-- Returns the input breakdown name if it's an unknown breakdown, NULL otherwise.
CREATE PERFETTO FUNCTION internal_invalid_name_or_null(name STRING)
RETURNS STRING AS
SELECT
  CASE
    WHEN
    $name not in (
      "GenerationToBrowserMain", "GenerationToRendererCompositor",
      "BrowserMainToRendererCompositor", "RendererCompositorQueueingDelay",
      "RendererCompositorToMain", "RendererCompositorProcessing",
      "RendererMainProcessing", "EndActivateToSubmitCompositorFrame",
      "SubmitCompositorFrameToPresentationCompositorFrame",
      "ArrivedInRendererCompositorToTermination",
      "RendererCompositorStartedToTermination",
      "RendererMainFinishedToTermination",
      "RendererCompositorFinishedToTermination",
      "RendererMainStartedToTermination",
      "RendererCompositorFinishedToBeginImplFrame",
      "RendererCompositorFinishedToCommit",
      "RendererCompositorFinishedToEndCommit",
      "RendererCompositorFinishedToActivation",
      "RendererCompositorFinishedToEndActivate",
      "RendererCompositorFinishedToSubmitCompositorFrame",
      "RendererMainFinishedToBeginImplFrame",
      "RendererMainFinishedToSendBeginMainFrame",
      "RendererMainFinishedToCommit", "RendererMainFinishedToEndCommit",
      "RendererMainFinishedToActivation", "RendererMainFinishedToEndActivate",
      "RendererMainFinishedToSubmitCompositorFrame",
      "BeginImplFrameToSendBeginMainFrame",
      "RendererCompositorFinishedToSendBeginMainFrame",
      "SendBeginMainFrameToCommit", "Commit",
      "EndCommitToActivation", "Activation")
      THEN $name
    ELSE NULL
  END;

-- Creates a view where each row contains information about one EventLatency event. Columns are duration of breakdowns.
-- In the result it will be something like this:
-- | event_latency_id | event_latency_ts | event_latency_dur | event_type       | GenerationToBrowserMainNs | BrowserMainToRendererCompositorNs |...|
-- |------------------|------------------|-------------------|------------------|----------------------------|------------------------------------|---|
-- | 123              | 1661947470       | 20                | 1234567          | 30                         | 50                                 |   |
--
-- @column event_latency_id                  The EventLatency ID.
-- @column event_latency_track_id            The EventLatency Track ID.
-- @column event_latency_ts                  Timestamp of EventLatency.
-- @column event_latency_dur                 Duration of EventLatency.
-- @column event_type                        Event type.
-- @column GenerationToRendererCompositorNs  Duration of the
--                                           GenerationToRendererCompositorNs
--                                           stage. All subsequent columns are
--                                           durations and named for their
--                                           respective stages.
-- @column GenerationToBrowserMainNs         Duration, see above.
-- @column BrowserMainToRendererCompositorNs Duration, see above.
-- @column RendererCompositorQueueingDelayNs Duration, see above.
-- @column RendererCompositorProcessingNs    Duration, see above.
-- @column RendererCompositorToMainNs        Duration, see above.
-- @column RendererMainProcessingNs          Duration, see above.
-- @column ArrivedInRendererCompositorToTerminationNs   Duration, see above.
-- @column RendererCompositorStartedToTerminationNs     Duration, see above.
-- @column RendererCompositorFinishedToTerminationNs    Duration, see above.
-- @column RendererMainStartedToTerminationNs           Duration, see above.
-- @column RendererMainFinishedToTerminationNs          Duration, see above.
-- @column BeginImplFrameToSendBeginMainFrameNs         Duration, see above.
-- @column RendererCompositorFinishedToSendBeginMainFrameNs Duration, see above.
-- @column RendererCompositorFinishedToBeginImplFrameNs     Duration, see above.
-- @column RendererCompositorFinishedToCommitNs         Duration, see above.
-- @column RendererCompositorFinishedToEndCommitNs      Duration, see above.
-- @column RendererCompositorFinishedToActivationNs     Duration, see above.
-- @column RendererCompositorFinishedToEndActivateNs    Duration, see above.
-- @column RendererCompositorFinishedToSubmitCompositorFrameNs Duration, see
--                                                             above.
-- @column RendererMainFinishedToBeginImplFrameNs       Duration, see above.
-- @column RendererMainFinishedToSendBeginMainFrameNs   Duration, see above.
-- @column RendererMainFinishedToCommitNs    Duration, see above.
-- @column RendererMainFinishedToEndCommitNs Duration, see above.
-- @column RendererMainFinishedToActivationNs           Duration, see above.
-- @column RendererMainFinishedToEndActivateNs          Duration, see above.
-- @column RendererMainFinishedToSubmitCompositorFrameNs       Duration, see
--                                                             above.
-- @column EndActivateToSubmitCompositorFrameNs         Duration, see above.
-- @column SubmitCompositorFrameToPresentationCompositorFrameNs Duration, see
--                                                              above.
-- @column SendBeginMainFrameToCommitNs      Duration, see above.
-- @column CommitNs                          Duration, see above.
-- @column EndCommitToActivationNs           Duration, see above.
-- @column ActivationNs                      Duration, see above.
-- @column unknown_stages_seen               List of any unknown stages.
CREATE VIEW chrome_event_latency_to_breakdowns
AS
SELECT
  event_latency_id,
  event_latency_track_id,
  event_latency_ts,
  event_latency_dur,
  event_type,
  max(CASE WHEN name = "GenerationToRendererCompositor" THEN dur END) AS GenerationToRendererCompositorNs,
  max(CASE WHEN name = "GenerationToBrowserMain" THEN dur END) AS GenerationToBrowserMainNs,
  max(CASE WHEN name = "BrowserMainToRendererCompositor" THEN dur END) AS BrowserMainToRendererCompositorNs,
  max(CASE WHEN name = "RendererCompositorQueueingDelay" THEN dur END) AS RendererCompositorQueueingDelayNs,
  max(CASE WHEN name = "RendererCompositorProcessing" THEN dur END) AS RendererCompositorProcessingNs,
  max(CASE WHEN name = "RendererCompositorToMain" THEN dur END) AS RendererCompositorToMainNs,
  max(CASE WHEN name = "RendererMainProcessing" THEN dur END) AS RendererMainProcessingNs,

  max(CASE WHEN name = "ArrivedInRendererCompositorToTermination" THEN dur END) AS ArrivedInRendererCompositorToTerminationNs,
  max(CASE WHEN name = "RendererCompositorStartedToTermination" THEN dur END) AS RendererCompositorStartedToTerminationNs,
  max(CASE WHEN name = "RendererCompositorFinishedToTermination" THEN dur END) AS RendererCompositorFinishedToTerminationNs,
  max(CASE WHEN name = "RendererMainStartedToTermination" THEN dur END) AS RendererMainStartedToTerminationNs,
  max(CASE WHEN name = "RendererMainFinishedToTermination" THEN dur END) AS RendererMainFinishedToTerminationNs,

  max(CASE WHEN name = "BeginImplFrameToSendBeginMainFrame" THEN dur END) AS BeginImplFrameToSendBeginMainFrameNs,
  max(CASE WHEN name = "RendererCompositorFinishedToSendBeginMainFrame" THEN dur END) AS RendererCompositorFinishedToSendBeginMainFrameNs,
  max(CASE WHEN name = "RendererCompositorFinishedToBeginImplFrame" THEN dur END) AS RendererCompositorFinishedToBeginImplFrameNs,
  max(CASE WHEN name = "RendererCompositorFinishedToCommit" THEN dur END) AS RendererCompositorFinishedToCommitNs,
  max(CASE WHEN name = "RendererCompositorFinishedToEndCommit" THEN dur END) AS RendererCompositorFinishedToEndCommitNs,
  max(CASE WHEN name = "RendererCompositorFinishedToActivation" THEN dur END) AS RendererCompositorFinishedToActivationNs,
  max(CASE WHEN name = "RendererCompositorFinishedToEndActivate" THEN dur END) AS RendererCompositorFinishedToEndActivateNs,
  max(CASE WHEN name = "RendererCompositorFinishedToSubmitCompositorFrame" THEN dur END) AS RendererCompositorFinishedToSubmitCompositorFrameNs,
  max(CASE WHEN name = "RendererMainFinishedToBeginImplFrame" THEN dur END) AS RendererMainFinishedToBeginImplFrameNs,
  max(CASE WHEN name = "RendererMainFinishedToSendBeginMainFrame" THEN dur END) AS RendererMainFinishedToSendBeginMainFrameNs,
  max(CASE WHEN name = "RendererMainFinishedToCommit" THEN dur END) AS RendererMainFinishedToCommitNs,
  max(CASE WHEN name = "RendererMainFinishedToEndCommit" THEN dur END) AS RendererMainFinishedToEndCommitNs,
  max(CASE WHEN name = "RendererMainFinishedToActivation" THEN dur END) AS RendererMainFinishedToActivationNs,
  max(CASE WHEN name = "RendererMainFinishedToEndActivate" THEN dur END) AS RendererMainFinishedToEndActivateNs,
  max(CASE WHEN name = "RendererMainFinishedToSubmitCompositorFrame" THEN dur END) AS RendererMainFinishedToSubmitCompositorFrameNs,

  max(CASE WHEN name = "EndActivateToSubmitCompositorFrame" THEN dur END) AS EndActivateToSubmitCompositorFrameNs,
  max(CASE WHEN name = "SubmitCompositorFrameToPresentationCompositorFrame" THEN dur END) AS SubmitCompositorFrameToPresentationCompositorFrameNs,
  max(CASE WHEN name = "SendBeginMainFrameToCommit" THEN dur END) AS SendBeginMainFrameToCommitNs,
  max(CASE WHEN name = "Commit" THEN dur END) AS CommitNs,
  max(CASE WHEN name = "EndCommitToActivation" THEN dur END) AS EndCommitToActivationNs,
  max(CASE WHEN name = "Activation" THEN dur END) AS ActivationNs,
  -- This column indicates whether there are unknown breakdowns.
  -- Contains: NULL if there are no unknown breakdowns, otherwise a list of unknown breakdows.
  group_concat(internal_invalid_name_or_null(name), ', ') AS unknown_stages_seen
FROM chrome_event_latency_breakdowns
GROUP BY event_latency_id;
