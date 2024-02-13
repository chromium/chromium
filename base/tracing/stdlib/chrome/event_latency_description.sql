-- Copyright 2023 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

-- Source of truth of the descriptions of EventLatency stages.
CREATE PERFETTO TABLE chrome_event_latency_stage_descriptions (
    -- The name of the EventLatency stage.
    name STRING,
    -- A description of the EventLatency stage.
    description STRING
) AS
WITH event_latency_descriptions(
  name,
  description)
AS (
VALUES
  ('TouchRendererHandlingToBrowserMain',
    'Interval between when the website handled blocking touch move to when ' ||
    'the browser UI thread started processing the input. Blocking touch ' ||
    'move happens when a touch event has to be handled by the website ' ||
    'before being converted to a scroll.'),
  ('GenerationToBrowserMain',
    'Interval between OS-provided hardware input timestamp to when the ' ||
    'browser UI thread began processing the input.'),
  ('GenerationToRendererCompositor',
    'Interval between OS-provided hardware input timestamp to when the ' ||
    'renderer compositor thread starts handling the artificial TOUCH_PRESS ' ||
    'browser injects in the kTouchScrollStarted event. See ' ||
    'PrependTouchScrollNotification for more info.'),
  ('BrowserMainToRendererCompositor',
    'Interval between when Browser UI thread starts to process the input to ' ||
    'renderer compositor thread starting to process it. This stage includes ' ||
    'browser UI thread processing, and task queueing times on the IO and ' ||
    'renderer compositor threads.'),
  ('RendererCompositorQueueingDelay',
    'Interval between when the input event is queued in the renderer ' ||
    'compositor and start of the BeginImplFrame producing a frame ' ||
    'containing this input.'),
  ('RendererCompositorToMain',
    'Interval between when the Renderer Compositor finishes processing the ' ||
    'event and when the Renderer Main (CrRendererMain) starts processing ' ||
    'the event, only seen when the compositor thread cannot handle the ' ||
    'scroll event by itself (known as "slow path"), usually caused by the ' ||
    'presence of blocking JS event listeners or complex page layout.'),
  ('RendererCompositorProcessing',
    'Interval corresponding to the Renderer Compositor thread processing ' ||
    'the frame updates.'),
  ('RendererMainProcessing',
    'Interval corresponding to the Renderer Main thread processing the ' ||
    'frame updates.'),
  ('EndActivateToSubmitCompositorFrame',
    'Interval that the Renderer Compositor waits for the GPU to flush a ' ||
    'frame to submit a new one.'),
  ('SubmitCompositorFrameToPresentationCompositorFrame',
    'Interval between the first Renderer Frame received to when the system ' ||
    'presented the fully composited frame on the screen. Note that on some ' ||
    'systems/apps this is incomplete/inaccurate due to lack of feedback ' ||
    'timestamps from the platform (Mac, iOS, Android Webview, etc).'),
  ('ArrivedInRendererCompositorToTermination',
    'Interval between when Renderer Compositor received the frame to when ' ||
    'this input was decided to either be ignored or merged into another ' ||
    'frame being produced. This could be a dropped frame, or just a normal ' ||
    'coalescing.'),
  ('RendererCompositorStartedToTermination',
    'Interval between when Renderer Compositor started processing the frame ' ||
    'to when this input was decided to either be ignored or merged into ' ||
    'another frame being produced. This could be a dropped frame, or just a ' ||
    'normal coalescing.'),
  ('RendererMainFinishedToTermination',
    'Interval between when Renderer Main finished processing the frame ' ||
    'to when this input was decided to either be ignored or merged into ' ||
    'another frame being produced. This could be a dropped frame, or just a ' ||
    'normal coalescing.'),
  ('RendererCompositorFinishedToTermination',
    'Interval between when Renderer Compositor finished processing the ' ||
    'frame to when this input was decided to either be ignored or merged ' ||
    'into another frame being produced. This could be just a normal ' ||
    'coalescing.'),
  ('RendererMainStartedToTermination',
    'Interval between when Renderer Main started processing the frame ' ||
    'to when this input was decided to either be ignored or merged into ' ||
    'another frame being produced. This could be a dropped frame, or just a ' ||
    'normal coalescing.'),
  ('RendererCompositorFinishedToBeginImplFrame',
    'Interval when Renderer Compositor has finished processing a vsync ' ||
    '(with input), but did not end up producing a CompositorFrame due to ' ||
    'reasons such as waiting on main thread, and is now waiting for the ' ||
    'next BeginFrame from the GPU VizCompositor.'),
  ('RendererCompositorFinishedToCommit',
    'Interval between when the Renderer Compositor has finished its work ' ||
    'and the current tree state will be committed from the Renderer Main ' ||
    '(CrRendererMain) thread.'),
  ('RendererCompositorFinishedToEndCommit',
    'Interval between when the Renderer Compositor finishing processing to ' ||
    'the Renderer Main (CrRendererMain) both starting and finishing the ' ||
    'commit.'),
  ('RendererCompositorFinishedToActivation',
    'Interval of activation without a previous commit (not as a stage with ' ||
    'ToEndCommit). Activation occurs on the Renderer Compositor Thread ' ||
    'after it has been notified of a fully committed RendererMain tree.'),
  ('RendererCompositorFinishedToEndActivate',
    'Interval when the Renderer Compositor has finished processing and ' ||
    'activating the Tree.'),
  ('RendererCompositorFinishedToSubmitCompositorFrame',
    'Interval when processing does not need to wait for a commit (can do an ' ||
    'early out) for activation and can go straight to providing the frame ' ||
    'to the GPU VizCompositor. The Renderer Compositor is waiting for the ' ||
    'GPU to flush a frame so that it can then submit a new frame.'),
  ('RendererMainFinishedToBeginImplFrame',
    'Interval when the input was sent first to the RendererMain thread and ' ||
    'now requires the Renderer Compositor to react, aka it is is waiting ' ||
    'for a BeginFrame signal.'),
  ('RendererMainFinishedToSendBeginMainFrame',
    'Interval during which the Renderer Main (CrRendererMain) thread is ' ||
    'waiting for BeginMainFrame.'),
  ('RendererMainFinishedToCommit',
    'Interval when the Renderer Main (CrRendererMain) is ready to commit ' ||
    'its work to the Renderer Compositor.'),
  ('BeginImplFrameToSendBeginMainFrame',
    'Interval during which the Renderer Compositor has received the ' ||
    'BeginFrame signal from the GPU VizCompositor, and now needs to send it ' ||
    'to the Renderer Main thread (CrRendererMain).'),
  ('RendererCompositorFinishedToSendBeginMainFrame',
    'Interval during which the Renderer Compositor is waiting for a ' ||
    'BeginFrame from the GPU VizCompositor, and it expects to have to do ' ||
    'work on the Renderer Main thread (CrRendererMain), so we are waiting ' ||
    'for a BeginMainFrame'),
  ('SendBeginMainFrameToCommit',
    'Interval when updates (such as HandleInputEvents, Animate, StyleUpdate ' ||
    'and LayoutUpdate) are updatedon the Renderer Main thread ' ||
    '(CrRendererMain).'),
  ('Commit',
    'Interval during which the Renderer Main thread (CrRendererMain) ' ||
    'commits updates back to Renderer Compositor for activation. ' ||
    'Specifically, the main thread copies its own version of layer tree ' ||
    'onto the pending tree on the compositor thread. The main thread is ' ||
    'blocked during the copying process.'),
  ('EndCommitToActivation',
    'Interval when the commit is ready and waiting for activation.'),
  ('Activation',
    'Interval when the layer trees and properties are on the pending tree ' ||
    'is pused to the active tree on the Renderer Compositor.'),
  ('SubmitToReceiveCompositorFrame',
    'Interval of the delay b/w Renderer Compositor thread sending ' ||
    'CompositorFrame and then GPU VizCompositorThread receiving the ' ||
    'CompositorFrame.'),
  ('ReceiveCompositorFrameToStartDraw',
    'Interval between the first frame received to when all frames (or ' ||
    'timeouts have occurred) and we start drawing. It can be blocked by ' ||
    'other processes (e.g to draw a toolbar it waiting for information from ' ||
    'the Browser) as it waits for timeouts or frames to be provided. This ' ||
    'is the tree of dependencies that the GPU VizCompositor is waiting for ' ||
    'things to arrive. That is creating a single frame for multiple ' ||
    'compositor frames. '),
  ('StartDrawToSwapStart',
    'Interval when all compositing sources are done, or compositing ' ||
    'deadline passes - the viz thread takes all the latest composited ' ||
    'surfaces and issues the software draw instructions to layer the ' ||
    'composited tiles, this substage ends when the swap starts on Gpu ' ||
    'CompositorGpuThread.'),
  ('SwapStartToBufferAvailable',
    'Interval that is a substage of stage "Swap" when the framebuffer ' ||
    'is prepared by the system and the fence Chrome waits on before ' ||
    'writing is signalled, and Chrome can start transferring the new frame.'),
  ('BufferAvailableToBufferReady',
    'Interval that is a Ssubstage of stage "Swap" when Chrome is ' ||
    'transferring a new frame to when it has finished completely sending a ' ||
    'frame to the framebuffer.'),
  ('BufferReadyToLatch',
    'Interval that is a substage of stage "Swap", when the system latches ' ||
    'and is ready to use the frame, and then it can get to work producing ' ||
    'the final frame.'),
  ('LatchToSwapEnd',
    'Intereval that is a substage of stage "Swap", when the latch has ' ||
    'finished until the frame is fully swapped and in the queue of frames ' ||
    'to be presented.'),
  ('SwapEndToPresentationCompositorFrame',
    'Interval that the frame is presented on the screen (and pixels became ' ||
    'visible).'))
SELECT
  name,
  description
FROM event_latency_descriptions;
