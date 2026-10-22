---
name: sync-model-processor
description: >-
  Reviews changes to Chromium Sync model internals in components/sync/model/,
  including ClientTagBasedDataTypeProcessor, ClientTagBasedRemoteUpdateHandler,
  ProcessorEntity, ProcessorEntityMetadata, and ProcessorEntityTracker. Use
  when modifying or reviewing processor state transitions, entity metadata
  tracking, conflict resolution handling, or commit/update flows in
  components/sync/model/. Don't use for data type bridge implementations (use
  sync-bridge) or standalone .proto reviews (use sync-proto-review).
---

# Sync Model Processor Internals

Changes to the processor in `components/sync/model/` affect every data type, so
state machine and metadata invariants must be preserved.

## Instructions

1. **Read Design Docs If Available**: Try to read the following design docs if
   present in the checkout (they live in the separate `chromium/website`
   repository and may not be available). Use them for architectural context and
   design intent, noting that for current API and state names the headers below
   take precedence:
   - `docs/website/site/developers/design-documents/sync/client-tag-based-data-type-processor/index.md`
   - `docs/website/site/developers/design-documents/sync/model-api/index.md`
2. **Read Relevant Headers and Contracts**: Read the headers and proto
   definitions relevant to the change:
   - `components/sync/model/client_tag_based_data_type_processor.h`
   - `components/sync/model/client_tag_based_remote_update_handler.h`
   - `components/sync/model/processor_entity.h`
   - `components/sync/model/processor_entity_metadata.h`
   - `components/sync/model/processor_entity_tracker.h`
   - `components/sync/model/data_type_sync_bridge.h`
   - `components/sync/model/data_type_local_change_processor.h`
   - `components/sync/protocol/entity_metadata.proto`
   - `components/sync/protocol/data_type_state.proto`
3. **Evaluate Changes**: Check the diff against the contracts in the headers
   above, the design docs (if available), and the core invariants below.

## Core Invariants

1. **Processor state machine**: Model readiness (`model_ready_to_sync_`,
   `model_error_`), metadata tracking (`entity_tracker_`), and sync engine
   connection (`worker_`) transition independently (`OnSyncStarting()` and
   `ModelReadyToSync()` may occur in either order):
   - *Uninitialized* (`!model_ready_to_sync_`): metadata is not loaded; `Put()`
     and `Delete()` must `CHECK`-fail.
   - *Not tracking* (`model_ready_to_sync_ && !entity_tracker_`): initial sync
     has not completed; `IsTrackingMetadata()` is false and local `Put()` /
     `Delete()` calls are ignored.
   - *Tracking* (`entity_tracker_ != nullptr`): initial sync has completed;
     `IsTrackingMetadata()` is true and local changes are recorded in metadata
     even while disconnected (`!worker_`).
   - *Syncing* (`entity_tracker_ && IsConnected()`): connected to `worker_`;
     commits and remote updates can flow.
2. **In-memory vs. persisted metadata**: `ProcessorEntityTracker` holds
   `DataTypeState` and `ProcessorEntity` (`EntityMetadata`) in memory, while
   `ProcessorEntity` also holds transient commit state
   (`commit_requested_sequence_number_`, cached `commit_data_`). Any mutation to
   durable metadata must be paired with a corresponding `MetadataChangeList`
   change passed to the bridge, whereas transient commit state must be cleared
   on disconnect or commit failure.
3. **Bridge contract boundaries**: Respect `DataTypeSyncBridge` capabilities and
   preconditions across all data types (`SupportsGetClientTag()`,
   `SupportsGetStorageKey()`, `SupportsIncrementalUpdates()`,
   `SupportsUniquePositions()`, commit-only types, and validating non-deletion
   updates with `IsEntityDataValid()` before calling `GetClientTag()` or
   `GetStorageKey()`).
