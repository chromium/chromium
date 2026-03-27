# Record Replay

This directory (`chrome/browser/record_replay`) contains the browser-process
coordinator code for the **Record Replay** feature in Chromium.

This system is used for observing, recording, and serializing user actions
(clicks, selects, text updates, Autofill interactions) on a web page, and
subsequently replaying them.

While `chrome/renderer/record_replay` provides the renderer-side implementation
to observe interactions and actuate the DOM, `chrome/browser/record_replay` is
where we assemble the recording, persist it, and manage the state of recording
and replay. It maintains the state machine (recording, replaying, or idle),
manages intra-process communication via Mojo, tracks multiple frames, integrates
with the Tab lifecycle, and handles durable storage of the recorded sessions.

## Multi-process architecture

The Record Replay system is coordinated across several distinct process
boundaries:

- **Browser (Coordinator)**: `chrome/browser/record_replay`
  - **`RecordReplayManager`**: Holds the state machine (`kIdle`, `kRecording`,
    `kReplaying`). It's owned by `RecordReplayClient` and observes
    `autofill::AutofillManager`.
  - **`Recorder` / `Replayer`**: Implement the actual recording management
    operations and load/save states for state files.
- **Renderer (Agent)**: `chrome/renderer/record_replay`
  - **`RecordReplayAgent`**: A `RenderFrameObserver` and
    `WebRecordReplayClient`. It listens directly to layout and DOM triggers
    (like mouse triggers, text updates) and formats them into Mojo structures
    upwards, and simulates back when commanded from the coordinator.
- **Common Helpers and Interfaces**: `chrome/common/record_replay`
  - **Mojo boundaries**: Identifiable in `record_replay.mojom`. Defines
    coordinates and commands like `DoClick`, `GetElementSelector`, etc.
  - **Feature Flags**: Centralized in `record_replay_features.h`.
- **Browser (UI)**: `chrome/browser/ui/record_replay`
  - **`SaveRecordingBubbleController`**: Manages UI elements like the "Save
    Recording" bubble shown when a session concludes.

## Integration with Chromium

To understand how the Record Replay system is initialized and connected:

### 1. Browser-side Instantiation (`ChromeRecordReplayClient`)

- **Location**: `chrome/browser/ui/tabs/tab_features.cc`
- **Mechanism**: It is initialized as a **Tab Feature** (`tabs::TabFeature`) in
  `TabFeatures::Init`.
- **Condition**: It is only created if the feature flag
  `record_replay::features::kRecordReplayBase` is enabled (and not on Android).
- **Lifecycle**: It lives tied to the `tabs::TabInterface` lifespan.

### 2. Renderer-side Instantiation (`RecordReplayAgent`)

- **Location**: `chrome/renderer/chrome_content_renderer_client.cc`
- **Mechanism**: It is created in
  `ChromeContentRendererClient::RenderFrameCreated` using
  `new record_replay::RecordReplayAgent(render_frame, associated_interfaces)`.
- **Lifecycle**: It acts as a `content::RenderFrameObserver` tied to the
  `RenderFrame`.

### 3. Mojo IPC Wiring

- The browser exposes the `RecordReplayDriver` interface to the renderer in
  `chrome/browser/chrome_content_browser_client_receiver_bindings.cc` inside
  `RegisterAssociatedInterfaceBindersForRenderFrameHost` by binding
  `ChromeRecordReplayClient::BindRecordReplayDriver`.

## Object Ownership & Lifecycle

The Record Replay system follows a hierarchical ownership model tied to
Chromium's core lifecycles:

### 1. Persistent Storage (`RecordingDataManager`)

- **Lifecycle**: Tied to the **`Profile`**. It is implemented as a
  **`ProfileKeyedService`**.
- **Cardinality**: Exactly **1 per Profile**.
- **Threading**: Managed on the **UI thread**, but performs database I/O
  asynchronously on a dedicated background sequence via `leveldb_proto`.

### 2. Tab-Level Coordination (`ChromeRecordReplayClient`)

- **Lifecycle**: Tied to the **`tabs::TabInterface`**. It is initialized as a
  **`tabs::TabFeature`**.
- **Cardinality**: Exactly **1 per Tab**.
- **Threading**: Operates entirely on the **UI thread**.
- **Ownership**: Owns the `RecordReplayManager` and `RecordReplayDriverFactory`.

### 3. State Management (`RecordReplayManager`)

- **Lifecycle**: Same as `ChromeRecordReplayClient` (tied to the Tab).
- **Cardinality**: **1 per Tab**.
- **Ownership**: Manages the active `Recorder` or `Replayer` instance. Only one
  of these can be active at any given time.

### 4. Frame-Level Communication (`RecordReplayDriver`)

- **Lifecycle**: Tied to a **`content::RenderFrameHost`**.
- **Cardinality**: **1 per Frame** within a Tab. Managed by
  `RecordReplayDriverFactory` (a `WebContentsObserver`).
- **Ownership**: Owned by `RecordReplayDriverFactory`. Instances are created
  when a frame is created and destroyed when it is deleted.

### 5. Active Sessions (`Recorder` and `Replayer`)

- **Lifecycle**: Transient. Created by `RecordReplayManager` when recording or
  replay starts, and destroyed when the session finishes or is cancelled.
- **Cardinality**: **0 or 1** active `Recorder` **OR** `Replayer` per Tab.

### 6. User Interface (`SaveRecordingBubbleController`)

- **Lifecycle**: Transient. Created when a recording session successfully
  finishes and needs user input for naming/saving.
- **Cardinality**: **0 or 1** active bubble per Tab.

## Threading & Sequencing

All coordinator logic in `chrome/browser/record_replay` runs on the **UI
thread**.

- **Mojo**: IPC calls between the Browser (UI thread) and Renderer process are
  handled via associated interfaces to ensure message ordering relative to other
  frame-bound messages.
- **Persistence**: Database operations in `RecordingDataManagerImpl` are
  dispatched to a background thread pool to avoid blocking the UI thread.
- **Replay Timing**: `Replayer` uses `base::OneShotTimer` on the UI thread to
  manage the delays between recorded actions.

## Upkeep / Guidelines for AI Agents

NOTE: For detailed information about each component, please refer to the
documentation in the respective header files (\*.h) within this directory.

> [!IMPORTANT] This document functions as a **living document**. AI agents
> introducing any major sub-features, changing process or Mojo interfaces, or
> solving significant ambiguities **MUST** update this file to reflect the
> updated mental model of the feature tree.
