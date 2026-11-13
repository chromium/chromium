---
name: components/sync/do-not-map-unsupported-sync-proto-fields-to-entities
description: Use when Sync serialization, deserialization, trimming helpers, sync bridges, feature flags, or shared .proto definitions handle protobuf fields or entity types that Chrome does not support.
rubric: design
severity: blocking
---

**Rule.** When writing or updating Sync serialization, deserialization, trimming
helpers, or sync bridges (such as in entity_sync_util.cc,
valuables_sync_util.cc, or valuable_sync_bridge.cc), do not create attribute
mappings, boilerplate trimming helpers, or premature base::Feature flags for
protobuf fields and entity types unsupported in Chrome. Define cross-platform
fields in the shared .proto definition with comments documenting that they are
unused in Chrome, and handle unsupported types directly as intentional no-ops
(e.g. `break;`) accompanied by explanatory comments. Only reject unrecognized
`oneof` variants in `IsEntityDataValid()` (returning `false`) in download-only
full-sync bridges (`SupportsIncrementalUpdates() == false`) that do not cache
unknown fields; incremental bridges must accept entities with valid identity in
`IsEntityDataValid()` and ignore unrecognized variants when updating the local
model.

**Why.** Sync protocol buffers are shared across multiple platforms (such as
Android, Google Wallet, and Chrome). Adding attribute mappings, dead trimming
helpers, or premature feature flags for types unsupported in Chrome adds dead
code, increases flag maintenance overhead, and risks mismanaging remote
specifics that should simply be ignored or preserved without processing. In
incremental bridges, however, returning `false` from `IsEntityDataValid()` drops
the remote update before tracking `EntityMetadata` or caching
`possibly_trimmed_base_specifics`, breaking forward-compatibility caching across
browser upgrades and making it hard to detect after an upgrade that the dropped
data needs to be redownloaded.

## Examples

Bad:

```cpp
// Mapping an unsupported proto field to a local entity attribute:
SET_OR_CLEAR_STRING_FIELD(entity, kVehicleIssuer, issuer_name, vehicle);

// Writing boilerplate trimming helpers for an unsupported sync type:
case AutofillValuableSpecifics::kEventTicket: {
  TrimEventTicket(*trimmed_specifics.mutable_event_ticket());
  if (trimmed_specifics.event_ticket().ByteSizeLong() == 0) {
    trimmed_specifics.clear_event_ticket();
  }
  break;
}

// Checking premature feature flags for unsupported entity types in sync bridge:
case sync_pb::AutofillValuableSpecifics::kEventTicket:
  return IsSyncWalletEventTicketsEnabled();
case sync_pb::AutofillValuableSpecifics::kTransitPass:
  return IsSyncWalletTransitPassesEnabled();
```

Good:

```cpp
// In autofill_valuable_specifics.proto: document that the field is unused in Chrome:
// The name of the issuer of the vehicle registration, e.g. "DMV".
// Note that this field is currently not used in Chrome.
optional string issuer_name = 9;

// In valuables_sync_util.cc: handle unsupported types as a no-op with a comment:
case AutofillValuableSpecifics::kEventTicket: {
  // Chrome does not support EventTicket.
  break;
}

// In an incremental sync bridge: accept entities with valid identity in
// IsEntityDataValid() so metadata and unknown fields are tracked:
case sync_pb::AutofillValuableSpecifics::kEventTicket:
case sync_pb::AutofillValuableSpecifics::kTransitPass:
case sync_pb::AutofillValuableSpecifics::VALUABLE_DATA_NOT_SET:
  return !autofill_valuable.id().empty();

// And ignore unsupported/unknown variants when updating the local model:
case sync_pb::AutofillValuableSpecifics::kEventTicket:
case sync_pb::AutofillValuableSpecifics::kTransitPass:
case sync_pb::AutofillValuableSpecifics::VALUABLE_DATA_NOT_SET:
  // Ignore entry types that Chrome does not support yet.
  break;
```
