---
name: general/document-trade-offs-for-os-persistence-and-sync-primitives
rubric: readability
severity: nit
---

**Rule.** When selecting or defaulting to a specific OS-level synchronization or
persistence primitive with non-trivial durability versus performance trade-offs
(such as choosing between file flush modes like F_BARRIERFSYNC and F_FULLFSYNC,
or fdatasync vs. fsync), document the distinction between the alternatives and
the rationale for the chosen approach in a code comment. Reference any
experimental validation, corruption metrics, or external precedent (such as
SQLite or OS documentation) supporting the choice.

**Why.** Different OS synchronization and persistence primitives provide vastly
different durability guarantees (e.g., write-barrier ordering vs. physical
storage cache flushes) and latency profiles. Documenting why a weaker or
performance-optimized durability mechanism was selected ensures future
maintainers understand the deliberate trade-off and do not inadvertently alter
consistency semantics.

## Examples

Bad:

```cpp
// This function uses F_BARRIERFSYNC to provide stronger
// guarantees than fsync().
if (!HANDLE_EINTR(fcntl(file_.get(), F_BARRIERFSYNC))) {
  return true;
}
```

Good:

```cpp
// This function uses F_BARRIERFSYNC to provide stronger guarantees than
// fsync(). The default behavior used to be `F_FULLFSYNC`. Changing it to
// F_BARRIERFSYNC for greatly reduced latency was extensively tried via
// experiment and showed no detectable sign of increased corruption in
// mechanisms that make use of this function. For similar discussions regarding
// rationale one can refer to the SQLite documentation where the default is to
// go directly to fsync. (See PRAGMA fullfsync)
if (!HANDLE_EINTR(fcntl(file_.get(), F_BARRIERFSYNC))) {
  return true;
}
```
