---
name: general/reserve-zeroth-protobuf-enum-value-for-unspecified
rubric: readability
severity: should_fix
---

**Rule.** In Protocol Buffer schemas (.proto), define the first enum value with
numeric tag 0 and suffix it with \_UNSPECIFIED or \_UNKNOWN (e.g.
FOO_UNSPECIFIED = 0;) to represent the default unset state. Start semantic
values at 1 or greater.

**Why.** Protobuf enum fields default to 0 when unset or uninitialized on the
wire. Reserving 0 for an explicit UNSPECIFIED or UNKNOWN sentinel prevents unset
fields from silently aliasing valid business logic states and ensures
forward-compatible decoding.

## Examples

Bad:

```cpp
enum TransferInputToVizResult {
  SUCCESSFULLY_TRANSFERRED = 0;
  INPUT_TRANSFER_HANDLER_NOT_FOUND_IN_MAP = 1;
}
```

Good:

```cpp
enum TransferInputToVizResult {
  TRANSFER_INPUT_TO_VIZ_RESULT_UNSPECIFIED = 0;
  SUCCESSFULLY_TRANSFERRED = 1;
  INPUT_TRANSFER_HANDLER_NOT_FOUND_IN_MAP = 2;
}
```
