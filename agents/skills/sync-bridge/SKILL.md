---
name: sync-bridge
description: >-
  Reviews and guides Chromium Sync Model API (DataTypeSyncBridge)
  implementations, including DataTypeStore persistence and forwards/backwards
  compatibility for sync specifics. Use when implementing or reviewing a
  DataTypeSyncBridge subclass, DataTypeStore usage, sync metadata persistence,
  conflict resolution, or specifics/model conversion code. Don't use for
  standalone .proto wire-format reviews (use sync-proto-review), internal
  processor changes in components/sync/model/ (use sync-model-processor), or
  initial new data type scaffolding (use sync-new-data-type).
---

# Sync Model API (`DataTypeSyncBridge`)

Changes to `DataTypeSyncBridge` implementations must follow the Sync Model API
contract to avoid data loss, duplicate commits, and data override by older
clients.

## Instructions

1. **Read Design Docs If Available**: Try to read the following design docs if
   present in the checkout (they live in the separate `chromium/website`
   repository and may not be available). Use them for design intent, noting that
   for current API names and signatures the headers below take precedence:
   - `docs/website/site/developers/design-documents/sync/model-api/index.md`
   - `docs/website/site/developers/design-documents/sync/sync-data-best-practices/index.md`
   - `docs/website/site/developers/design-documents/sync/old-sync-clients-data-override-protection/index.md`
2. **Read API Contracts and Guidelines**: Read the relevant headers and protocol
   guidelines:
   - `components/sync/model/data_type_sync_bridge.h`
   - `components/sync/model/data_type_local_change_processor.h`
   - `components/sync/model/data_type_store.h` (when `DataTypeStore` is used)
   - `components/sync/protocol/README.md` (when specifics or proto conversion is
     touched)
3. **Inspect Companion Code**: If the change modifies a `DataTypeSyncBridge` or
   specifics/model conversion code, check the corresponding
   `components/sync/protocol/*_specifics.proto` definition (and vice versa) to
   verify end-to-end compatibility.
4. **Evaluate Changes**: Check the diff against the contracts and documentation
   above and the checklist below.

## Review Checklist

### Persistence & Startup

1. **Atomic data and metadata writes**: Commit domain data and sync metadata
   (`MetadataChangeList`) in the same storage transaction (one
   `DataTypeStore::CommitWriteBatch()` call or one `sql::Transaction`). When
   using `DataTypeStore`, pass the existing `MetadataChangeList` to
   `store_->CreateWriteBatch(std::move(metadata_change_list))`, or for local
   writes pass `batch->GetMetadataChangeList()` to `change_processor()->Put()` /
   `Delete()` (do not call the deprecated zero-argument `CreateWriteBatch()`).
2. **Startup ordering**: Load persisted metadata and call
   `change_processor()->ModelReadyToSync(std::move(metadata_batch))` before any
   `Put()` / `Delete()`. The bridge must be able to handle `MergeFullSyncData()`
   or `ApplyIncrementalSyncChanges()` synchronously right after that call. On
   startup errors, call `change_processor()->ReportError()` **instead of**
   `ModelReadyToSync()`.
3. **Gate local changes on `IsTrackingMetadata()`**: Never gate `Put()` /
   `Delete()` on whether sync is active or connected. `IsTrackingMetadata()` is
   true (and local changes must be recorded) even while disconnected or paused.
4. **Identifiers**: `GetClientTag()` must depend only on immutable fields.
   `GetStorageKey()` must return a stable, non-empty local key; if keys are only
   assigned on insertion (`SupportsGetStorageKey()` returns false), new remote
   entities arrive with an empty storage key and the bridge must call
   `change_processor()->UpdateStorageKey()` for each of them.

### Sync Callbacks

1. **`MergeFullSyncData()`**: Write remote-only entities locally, upload
   local-only entities via `change_processor()->Put()` (using the passed
   `metadata_change_list`) before returning, merge entities present on both
   sides, and commit data and metadata together.
2. **Error reporting**: `MergeFullSyncData()` and
   `ApplyIncrementalSyncChanges()` report failures by returning a `ModelError`
   (`std::nullopt` on success), not by calling `ReportError()`. Everywhere else
   (startup, `GetDataForCommit()`, `GetAllDataForDebugging()`, async storage
   callbacks), call `change_processor()->ReportError()` (and return `nullptr`
   from `GetDataForCommit()` / `GetAllDataForDebugging()`).
3. **No re-entrant `Put()` / `Delete()`**: Never call
   `change_processor()->Put()` or `Delete()` from
   `ApplyIncrementalSyncChanges()` (or from observers notified within it), as
   this causes commit ping-pong between clients.
4. **`ApplyDisableSyncChanges()`**: The processor already fills
   `delete_metadata_change_list` to clear every entity's metadata and
   `DataTypeState`. Commit it (as the default implementation does) or wipe all
   data and metadata at once (e.g.
   `store_->DeleteAllDataAndMetadata(std::move(delete_metadata_change_list), ...)`
   or clearing SQL tables), rather than iterating entities to call
   `ClearMetadata()` again.
5. **`ResolveConflict()`**: Only called when both a local unsynced change and a
   different remote change exist. Overrides of the default (remote wins unless
   it is a deletion) need a clear data-type-specific reason; never resolve
   conflicts ad hoc inside `ApplyIncrementalSyncChanges()`.

### Forwards & Backwards Compatibility

1. **Defensive deserialization**: `IsEntityDataValid()` must check the specifics
   field and every proto field that `GetClientTag()`, `GetStorageKey()`,
   `MergeFullSyncData()`, or `ApplyIncrementalSyncChanges()` assumes is present.
   Incoming remote data must never `CHECK`-fail on missing optional fields,
   `FOO_UNSPECIFIED` enums, or unrecognized `oneof` variants (`NOT_SET`). Do not
   reject unknown `oneof` subtypes in `IsEntityDataValid()` (returning `false`
   drops the update without tracking its sync metadata), and do not locally
   modify entities whose `oneof` variant was `NOT_SET`.
2. **`TrimAllSupportedFieldsFromRemoteSpecifics()`**: It returns a copy of the
   passed specifics with every field the current binary supports cleared; the
   processor caches the result as `possibly_trimmed_base_specifics`.
   - Do not clear fields guarded by a disabled feature flag or only partially
     implemented; otherwise flag-off clients wipe data written by flag-on
     clients.
   - Keep clearing deprecated fields.
   - After clearing supported child fields of a submessage, clear the submessage
     itself if its `ByteSizeLong()` is `0u`.
   - Keep it in sync with the proto via `LINT.IfChange` / `LINT.ThenChange`.
   - Locally built specifics should satisfy
     `DCHECK_EQ(TrimAllSupportedFieldsFromRemoteSpecifics(specifics).ByteSizeLong(), 0u)`,
     which catches supported fields missing from the trimming function.
3. **Merge cached fields into outgoing specifics**: Outgoing specifics must
   start from
   `change_processor()->GetPossiblyTrimmedRemoteSpecifics(storage_key)` both
   before calling `Put()` and in `GetDataForCommit()`. `Put()` immediately
   recomputes the cached `possibly_trimmed_base_specifics` from the passed data,
   so merging only in `GetDataForCommit()` loses unknown fields on the first
   local edit.
4. **Browser upgrade**: Newly supported fields previously cached in
   `possibly_trimmed_base_specifics` must surface locally without waiting for a
   remote update, using one of three patterns:
   - *Structured local storage* (only known fields are persisted, e.g. SQL
     columns): on startup, check whether
     `TrimAllSupportedFieldsFromRemoteSpecifics()` trims anything from the
     cached `possibly_trimmed_base_specifics()`, and if so force an initial sync
     by deleting all persisted sync metadata (`DataTypeState` and every
     `EntityMetadata`) and passing an empty `MetadataBatch` to
     `ModelReadyToSync()`. Flag clearing only `DataTypeState`: the processor
     then treats the remaining entity metadata as invalid and calls
     `ApplyDisableSyncChanges()`, which may delete local data.
   - *Proto persistence in `DataTypeStore` + startup metadata merge* (only known
     fields are stored in the local data record): on startup (e.g. in
     `OnReadAllDataAndMetadata`), start from the entity's
     `possibly_trimmed_base_specifics()` in `MetadataBatch` and `MergeFrom()`
     the stored local proto on top.
   - *Full proto persistence in `DataTypeStore`* (the whole specifics proto,
     including `unknown_fields()`, is stored in `DataTypeStore`): local writes
     to the store (e.g. `WriteData()`) must also start from
     `GetPossiblyTrimmedRemoteSpecifics()`, otherwise unknown fields are lost
     from the store and never resurface after an upgrade.
