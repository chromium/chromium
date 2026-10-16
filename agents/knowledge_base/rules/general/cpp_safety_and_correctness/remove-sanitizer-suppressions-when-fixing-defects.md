---
name: general/remove-sanitizer-suppressions-when-fixing-defects
rubric: correctness
severity: should_fix
---

**Rule.** When modifying code to resolve undefined behavior, alignment faults,
type confusion, memory safety defects, or data races previously silenced by a
sanitizer suppression or ignorelist, remove the corresponding entry from the
sanitizer configuration file (e.g., tools/ubsan/ignorelist.txt,
tools/msan/ignorelist.txt, tools/cfi/ignores.txt, or
tools/tsan/suppressions.txt) in the same CL. Verify that all relevant sanitizer
trybots pass cleanly.

**Why.** Sanitizer ignorelists and suppression files are temporary workarounds
to keep CI green while bugs are tracked. When a code defect is resolved,
retaining its suppression leaves dead entries in tooling files and prevents CI
trybots and continuous fuzzers from verifying the fix and catching future
regressions in that code path.

## Examples

Bad:

```cpp
// base/pickle.cc fixes undefined behavior (misaligned header access):
Pickle::Header header;
byte_span_from_ref(header).copy_from_nonoverlapping(
    data.first(sizeof(header)));

// tools/ubsan/ignorelist.txt (suppression is left in place):
# https://crbug.com/363225486
[alignment]
fun:*base*Pickle*
```

Good:

```cpp
// base/pickle.cc fixes undefined behavior (misaligned header access):
Pickle::Header header;
byte_span_from_ref(header).copy_from_nonoverlapping(
    data.first(sizeof(header)));

// tools/ubsan/ignorelist.txt (obsolete suppression entry is removed in the same CL):
-# https://crbug.com/363225486
-[alignment]
-fun:*base*Pickle*
```
