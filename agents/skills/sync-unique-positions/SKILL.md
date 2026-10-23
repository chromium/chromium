---
name: sync-unique-positions
description: >-
  Guides implementing and reviewing entity ordering in Chrome Sync using Unique
  Positions (sync_pb::UniquePosition) in DataTypeSyncBridge and specifics
  protobufs. Use when a Sync data type requires relative ordering of entities
  across devices, or when adding, modifying, or reviewing UniquePosition,
  SupportsUniquePositions, GetUniquePosition, UniquePositionBefore/After/Between,
  or ApplyIncrementalSyncChanges ordering logic. Don't use for unordered Sync
  data types.
---

# Sync Unique Positions

When a Sync data type requires consistent ordering of entities across devices,
use `sync_pb::UniquePosition` (`syncer::UniquePosition` in C++) rather than
integer indices. Because `UniquePosition` represents arbitrary-precision
relative positions between neighbors, inserting or moving an entity only commits
that single entity to the server without re-indexing or re-uploading neighboring
entities (reducing server traffic and conflict risk).

Design document (use when citing guidelines in reviews):
[Chrome Sync Unique Position API](https://chromium.googlesource.com/chromium/src/+/main/docs/website/site/developers/design-documents/sync/unique-positions/index.md)

______________________________________________________________________

## 1. Bridge & Protobuf Setup

1. **Protocol Buffer**: Add an `optional UniquePosition unique_position` field
   (`import "components/sync/protocol/unique_position.proto";`) to the data
   type's specifics proto.
2. **Bridge Overrides**: In `DataTypeSyncBridge`, override
   `SupportsUniquePositions()` to return `true`, and override
   `GetUniquePosition(const sync_pb::EntitySpecifics&)` to extract the
   `sync_pb::UniquePosition` from specifics (or return a default
   `sync_pb::UniquePosition()` for entities that do not require ordering).
3. **Validation (`IsEntityDataValid()`)**:
   - Reject remote entities with missing or corrupt positions in
     `IsEntityDataValid()` by checking
     `syncer::UniquePosition::FromProto(...).IsValid()` on ordered entities.
   - Only if a migrating data type must accept legacy entities without
     `unique_position` for backwards compatibility, omit the
     `IsEntityDataValid()` check and guard `UniquePosition::IsValid()` before
     calling `LessThan()` (which `DCHECK`s `IsValid()` on both operands).
4. **Persistence, Startup & Commits**:
   - `ClientTagBasedDataTypeProcessor` persists `unique_position` in
     `sync_pb::EntityMetadata`, so the local model and storage do not need to
     store `UniquePosition` values directly.
   - **Startup ordering**: If local storage does not persist item ordering
     across restarts, reconstruct the order on startup using `unique_position()`
     from `metadata_batch->GetAllMetadata()` (`syncer::EntityMetadataMap`).
     Guard `IsValid()` before calling `LessThan()` in case an entity has no
     metadata entry (e.g., when sync is disabled).
   - **`GetDataForCommit()` and `GetAllDataForDebugging()`**: Populate the
     specifics' `unique_position` field from processor metadata via
     `change_processor()->GetUniquePositionForStorageKey(storage_key)`.

______________________________________________________________________

## 2. Generating Positions for Local Changes

Generate positions using `DataTypeLocalChangeProcessor`'s `UniquePosition*()`
helpers rather than calling `syncer::UniquePosition` static generators directly,
and populate the returned `sync_pb::UniquePosition` on `entity_data->specifics`
**before** calling `change_processor()->Put()` (which calls
`bridge_->GetUniquePosition()` to save it into `EntityMetadata`).

### Preconditions & Contracts

- **Metadata tracking must be active**: All `UniquePosition*()` and
  `GetUniquePositionForStorageKey()` methods `CHECK(IsTrackingMetadata())`. Only
  call them (and `Put()`) when `change_processor()->IsTrackingMetadata()` is
  true.
- **`target_client_tag_hash` must identify the target entity**: Pass the
  `ClientTagHash` of the entity being positioned (not a neighbor's). The
  processor uses it for deterministic tie-breaking across clients and to reuse
  the entity's existing position when it already satisfies the requested order.
- **Neighbors must be tracked in ascending order**: `storage_key_before` and
  `storage_key_after` must already be tracked locally by the processor with
  valid positions (via a prior local `Put()` or remote update; no server
  round-trip is required — otherwise `Sync.DataTypeUniquePositionError` is
  logged and an initial position is generated). For `UniquePositionBetween()`,
  `position_before.LessThan(position_after)` must hold (otherwise
  `Sync.DataTypeUniquePositionIncorrectOrder` is logged). When inserting
  multiple local items in a loop, call `UniquePosition*()` and `Put()` for each
  item sequentially so each neighbor is tracked before the next item references
  it.

______________________________________________________________________

## 3. Remote Updates (`ApplyIncrementalSyncChanges`)

### The Post-Batch Metadata Pitfall

`ClientTagBasedDataTypeProcessor` updates in-memory `EntityMetadata` for **all**
entities in an incoming update batch *before* invoking
`bridge->ApplyIncrementalSyncChanges()`.

Consequently, calling `change_processor()->GetUniquePositionForStorageKey(key)`
inside `ApplyIncrementalSyncChanges()` returns the **new (post-batch)**
`UniquePosition` for every entity in `entity_changes`, even for `ACTION_UPDATE`
entities that have not yet been moved in the local model. Comparing an incoming
item against un-moved model neighbors using their post-batch positions produces
wrong ordering.

### Recommended Approach (Right-to-Left Incremental Application)

1. **Sort ordered additions and updates (`ACTION_ADD` / `ACTION_UPDATE`) in
   reverse `UniquePosition` order (right to left)** using
   `rhs_pos.LessThan(lhs_pos)` (never `!lhs_pos.LessThan(rhs_pos)`, which
   returns `true` when `lhs == rhs` and violates strict weak ordering; guard
   `IsValid()` if legacy invalid positions are possible). Deletions
   (`ACTION_DELETE`) have no `UniquePosition` and can be applied in any order.
2. **Track entities with pending model updates (`storage_keys_to_update`)**.
3. **Apply ordered changes one by one in reverse order**:
   - Scan the model from left to right to find the first item with
     `local_pos >= remote_pos` (`!local_pos.LessThan(remote_pos)`), **skipping
     any local item whose key is still in `storage_keys_to_update`** (since its
     processor metadata already holds the new position while its model slot has
     not moved yet).
   - Applying right-to-left ensures each item's right neighbor in the batch is
     applied and removed from `storage_keys_to_update` first, acting as an
     anchor so items with unchanged positions do not temporarily jump past
     still-pending right neighbors.
   - In an indexed container (e.g., `std::vector`), if moving an existing item
     from `current_index < insert_before`, use `insert_before - 1` to account
     for the left shift when `current_index` is removed.
   - Erase `change->storage_key()` from `storage_keys_to_update` **after**
     updating the model.

```cpp
size_t MySyncBridge::PositionToInsertRemoteItem(
    const syncer::UniquePosition& remote_pos,
    const std::set<std::string>& storage_keys_to_ignore) const {
  for (size_t i = 0; i < model_->GetItemCount(); ++i) {
    const std::string& key = model_->GetStorageKeyAt(i);
    if (storage_keys_to_ignore.contains(key)) {
      continue;
    }
    syncer::UniquePosition local_pos = syncer::UniquePosition::FromProto(
        change_processor()->GetUniquePositionForStorageKey(key));
    if (!local_pos.LessThan(remote_pos)) {  // remote_pos <= local_pos
      return i;
    }
  }
  return model_->GetItemCount();
}

std::optional<syncer::ModelError> MySyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  std::vector<const syncer::EntityChange*> ordered_changes;
  std::set<std::string> storage_keys_to_update;

  for (const std::unique_ptr<syncer::EntityChange>& change : entity_changes) {
    if (change->type() == syncer::EntityChange::ACTION_DELETE) {
      model_->Delete(change->storage_key());
    } else {
      ordered_changes.push_back(change.get());
      storage_keys_to_update.insert(change->storage_key());
    }
  }

  // Sort additions and updates in reverse UniquePosition order (right to left).
  std::ranges::sort(
      ordered_changes,
      [this](const syncer::EntityChange* lhs, const syncer::EntityChange* rhs) {
        return syncer::UniquePosition::FromProto(
                   GetUniquePosition(rhs->data().specifics))
            .LessThan(syncer::UniquePosition::FromProto(
                GetUniquePosition(lhs->data().specifics)));
      });

  for (const syncer::EntityChange* change : ordered_changes) {
    const size_t insert_before = PositionToInsertRemoteItem(
        syncer::UniquePosition::FromProto(
            GetUniquePosition(change->data().specifics)),
        storage_keys_to_update);
    if (std::optional<size_t> current_index =
            model_->GetIndexOfKey(change->storage_key())) {
      const size_t target_index =
          insert_before > *current_index ? insert_before - 1 : insert_before;
      model_->UpdateAndMoveAt(target_index, change->storage_key(),
                              change->data());
    } else {
      model_->InsertAt(insert_before, change->storage_key(), change->data());
    }
    storage_keys_to_update.erase(change->storage_key());
  }

  return CommitMetadataAndData(std::move(metadata_change_list));
}
```

### Alternative Approach (Post-Update Full Re-Sort)

If the local model supports bulk reordering and does not require per-item move
notifications at the exact destination index during incremental updates, the
bridge can apply all deletions, updates (in place), and additions (appended)
first, and then sort the model once at the end of
`ApplyIncrementalSyncChanges()` using
`change_processor()->GetUniquePositionForStorageKey(key)` (`O(N log N)`).

______________________________________________________________________

## 4. Testing Guidelines

- **Mocking Post-Batch Metadata in Unit Tests**: Because the real processor
  updates metadata *before* calling `ApplyIncrementalSyncChanges()`, unit tests
  using `MockDataTypeLocalChangeProcessor` must set
  `ON_CALL(mock_processor(), GetUniquePositionForStorageKey(key)).WillByDefault(Return(new_pos))`
  *before* invoking `bridge()->ApplyIncrementalSyncChanges()`.
- **Unit Test Coverage**:
  - Local insertions and moves invoke the processor's `UniquePosition*()`
    helpers and call `Put()` **only** for the inserted/moved entity (`Times(0)`
    for neighbors).
  - Remote updates converge to the expected order across shuffled arrival
    permutations, mixed `ACTION_DELETE` + `ACTION_UPDATE` batches, and updates
    with unchanged positions (verifying via model observers that items are not
    temporarily reordered mid-batch).
- **Two-Client Sync Integration Tests (`SyncTest(TWO_CLIENT)`)**: Verify that
  local insertions, moves (to beginning, middle, and end), and concurrent
  updates on both clients converge to identical entity ordering.
