---
name: components/sync/retain-proto-visitors-for-deprecated-sync-fields
description: Use when adding, modifying, deprecating, or removing fields in Sync protobuf messages, or editing their VISIT_PROTO_FIELDS entries in components/sync/protocol/proto_visitors.h.
rubric: correctness
severity: blocking
---

**Rule.** When adding, modifying, or deprecating Sync protobuf messages and
their corresponding VISIT_PROTO_FIELDS implementations in
components/sync/protocol/proto_visitors.h, ensure all fields defined in the
protobuf schema are accurately visited using matching VISIT macros (VISIT,
VISIT_BYTES, VISIT_ENUM, VISIT_SECRET, VISIT_REP, VISIT_REP_BYTES) and avoid
referencing non-existent proto fields. When deprecating fields while retaining
their definitions in .proto files (e.g. marked [deprecated = true]), retain
their VISIT macro entries in proto_visitors.h until the field is completely
removed and replaced with a reserved tag and name.

**Why.** proto_visitors.h defines reflection helpers (VisitProtoFields) used
across Sync for memory usage estimation, serialization, and debugging tools
(e.g. chrome://sync-internals). Omitting fields, referencing non-existent
fields, or prematurely removing deprecated fields causes memory accounting
drift, broken serialization, and tool inaccuracies.

## Examples

Bad:

```cpp
// In components/sync/protocol/proto_visitors.h

// 1. Mismatched or omitted proto fields (e.g. transit_pass_type does not exist, passenger_name omitted):
VISIT_PROTO_FIELDS(const sync_pb::TransitPass& proto) {
  VISIT(agency_name);
  VISIT(transit_pass_number);
  VISIT(transit_pass_type);
  VISIT(agency_logo);
}

// 2. Removing VISIT entry while field remains defined as deprecated in .proto:
VISIT_PROTO_FIELDS(const sync_pb::ContactInfoSpecifics& proto) {
  VISIT(initial_creator_id);
  // BAD: Removed VISIT(last_modifier_id) while field is still in .proto schema.
  VISIT(name_first);
}
```

Good:

```cpp
// In components/sync/protocol/proto_visitors.h

// 1. Accurately mirror all defined proto fields with matching visitor macros:
VISIT_PROTO_FIELDS(const sync_pb::TransitPass& proto) {
  VISIT(agency_name);
  VISIT(passenger_name);
  VISIT(transit_pass_number);
  VISIT(agency_logo);
}

// 2. Retain VISIT entry for fields marked [deprecated = true]:
VISIT_PROTO_FIELDS(const sync_pb::ContactInfoSpecifics& proto) {
  VISIT(initial_creator_id);
  VISIT(last_modifier_id);
  VISIT(name_first);
}
```
