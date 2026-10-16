---
name: general/use-debug-alias-for-cstr-to-record-strings-in-crash-dumps
rubric: correctness
severity: should_fix
---

**Rule.** Do not pass object members, pointers, or std::string_view variables
directly to base::debug::Alias() when attempting to record string contents or
heap data in crash dumps. Copy the string into a stack-allocated buffer using
DEBUG_ALIAS_FOR_CSTR (or DEBUG_ALIAS_FOR_U16CSTR / DEBUG_ALIAS_FOR_WCHARCSTR)
with a null-terminated string so the character data is preserved in minidump
stack memory.

**Why.** Crash dumps and DumpWithoutCrashing() reports by default capture only
stack memory and registers. Calling base::debug::Alias() on a member variable or
pointer/string_view only aliases the pointer or local metadata on the stack,
omitting the underlying heap- or mapped-string characters.

## Examples

Bad:

```cpp
if (std::ranges::contains(kKnownBadHistogramsHashes, name_)) {
  base::debug::Alias(&name_);
  base::debug::DumpWithoutCrashing();
}
```

Good:

```cpp
if (std::ranges::contains(kKnownBadHistogramsHashes, name_)) {
  DEBUG_ALIAS_FOR_CSTR(hist_name, std::string(name_).c_str(), 32);
  base::debug::DumpWithoutCrashing();
}
```
