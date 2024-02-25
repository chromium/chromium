-- Copyright 2023 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

INCLUDE PERFETTO MODULE chrome.event_latency_description;

-- Source of truth of the descriptions of EventLatency-based scroll jank causes.
CREATE PERFETTO TABLE chrome_scroll_jank_cause_descriptions (
  -- The name of the EventLatency stage.
  event_latency_stage STRING,
  -- The process where the cause of scroll jank occurred.
  cause_process STRING,
  -- The thread where the cause of scroll jank occurred.
  cause_thread STRING,
  -- A description of the cause of scroll jank.
  cause_description STRING
) AS
WITH cause_descriptions(
  event_latency_stage,
  cause_process,
  cause_thread,
  cause_description)
AS (
VALUES
  ('GenerationToBrowserMain', 'Browser', 'CrBrowserMain',
    'This also corresponds to a matching InputLatency::TouchMove. Key ' ||
    'things to look for: Browser Main thread (CrBrowserMain) is busy, often ' ||
    'running tasks. The true cause can be confirmed by checking which tasks ' ||
    'are being run on CrBrowserMain, or checking any ScopedBlockingCall ' ||
    'slices during this stage from a ThreadPoolForegroundWorker, or ' ||
    'checking if the NetworkService is busy. Common causes may include page' ||
    'navigations (same document and new pages), slow BeginMainFrames, and ' ||
    'Java Choreographer slowdowns.'),
  ('RendererCompositorQueueingDelay', 'Renderer', 'Compositor',
    'The renderer needs to decide to produce a frame in response to a ' ||
    'BeginFrame signal. Sometimes it can not because it is waiting on the ' ||
    'RendererMain thread to do touch targeting or javascript handling or ' ||
    'other such things causing a long queuing delay after it has already ' ||
    'started the scroll (so the TouchStart has been processed).'),
  ('RendererCompositorQueueingDelay', 'GPU', 'VizCompositorThread',
    'Waiting for a BeginFrame to be sent. Key things to look for: check if ' ||
    'a fling occurred before or during the scroll; flings produce a single ' ||
    'input and result in multiple inputs coalescing into a single frame.'),
  ('ReceiveCompositorFrameToStartDraw', 'GPU', 'VizCompositorThread',
    'A delay when the VizCompositor is waiting for the frame, but may be ' ||
    'connected to other processes and threads. Key things to look for: ' ||
    'check the BeginFrame task that finished during this EventLatency. The ' ||
    'VizCompositor holds onto the frame/does not send it on. Alternately ' ||
    'the system may be holding on to the buffer.'),
  ('ReceiveCompositorFrameToStartDraw', 'GPU', 'CrGpuMain',
    'Key things to look for: if the GPU Main thread is busy, and does not ' ||
    'release the buffer; specific causes will be on the GPU Main thread. If ' ||
    'this thread is not busy, the buffer may be held by the system instead.'),
  ('ReceiveCompositorFrameToStartDraw', 'Browser', 'CrBrowserMain',
    'Key things to look for: the toolbar on the Browser may be blocked by ' ||
    'other tasks.'),
  ('BufferReadyToLatch', 'GPU', 'VizCompositorThread',
    'Often a scheduling issue. The frame was submitted, but missed the ' ||
    'latch in the system that was received from the previous frame. The ' ||
    'system only latches a buffer once per frame; when the latch deadline ' ||
    'is missed, the system is forced to wait for another vsync interval to ' ||
    'latch again. Key things to look for: whether the event duration before ' ||
    'BufferReadyToLatch stage of the previous EventLatency is longer or ' ||
    'shorter than the event duration before BufferReadyToLatch in the ' ||
    'current EventLatency. If this duration is longer, then this is a ' ||
    'System problem. If this duration is shorter, then it is a Chrome ' ||
    'problem. The previous frame may have been drawn too quickly, or the ' ||
    'GPU may be delayed.'),
  ('SwapEndToPresentationCompositorFrame', 'GPU', 'VizCompositorThread',
    'May be attributed to a scheduling issue as with BufferReadyToLatch. ' ||
    'The frame was submitted, but missed the latch in the system that was ' ||
    'received from the previous frame. The system only latches a buffer ' ||
    'once per frame; when the latch deadline is missed, the system is ' ||
    'forced to wait for another vsync interval to latch again. Key things ' ||
    'to look for: whether the event duration before BufferReadyToLatch ' ||
    'stage of the previous EventLatency is longer or shorter than the event ' ||
    'duration before BufferReadyToLatch in the current EventLatency. If ' ||
    'this duration is longer, then this is a System problem. If this ' ||
    'duration is shorter, then it is a Chrome problem. The previous frame ' ||
    'may have been drawn too quickly, or the GPU may be delayed.'),
  ('SwapEndToPresentationCompositorFrame', 'GPU', 'CrGpuMain',
    'Key things to look for: whether StartDrawToBufferAvailable is also ' ||
    'present during this EventLatency. If so, then the GPU main thread may ' ||
    'be descheduled or busy. If surfaceflinger is available, check there as ' ||
    'well.'),
  ('SwapEndToPresentationCompositorFrame', 'GPU', 'surfaceflinger',
    'Key things to look for: whether StartDrawToBufferAvailable is also ' ||
    'present during this EventLatency. If so, then the VizCompositor has ' ||
    'not received a signal from surfaceflinger to start writing into the ' ||
    'buffer.'))
SELECT
  event_latency_stage,
  cause_process,
  cause_thread,
  cause_description
FROM cause_descriptions;

-- Combined description of scroll jank cause and associated event latency stage.
CREATE PERFETTO VIEW chrome_scroll_jank_causes_with_event_latencies(
  -- The name of the EventLatency stage.
  name STRING,
  -- Description of the EventLatency stage.
  description STRING,
  -- The process name that may cause scroll jank.
  cause_process STRING,
  -- The thread name that may cause scroll jank. The thread will be on the
  -- cause_process.
  cause_thread STRING,
  -- Description of the cause of scroll jank on this process and thread.
  cause_description STRING
) AS
SELECT
  stages.name,
  stages.description,
  causes.cause_process,
  causes.cause_thread,
  causes.cause_description
FROM chrome_event_latency_stage_descriptions stages
LEFT JOIN chrome_scroll_jank_cause_descriptions causes
    ON causes.event_latency_stage = stages.name;
