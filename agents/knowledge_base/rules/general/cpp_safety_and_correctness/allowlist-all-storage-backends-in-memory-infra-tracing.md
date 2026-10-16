---
name: general/allowlist-all-storage-backends-in-memory-infra-tracing
rubric: safety
severity: should_fix
---

**Rule.** When adding dump providers or allocator dump name patterns to
memory_infra_background_allowlist.cc, ensure the dumps are verified to have
negligible processor and memory overhead in background mode. Additionally, use
canonical subsystem names (such as indexed_db rather than index_db) and ensure
all supported storage backend variants (such as LevelDB and SQLite) are
allowlisted across counterpart subsystems.

**Why.** Background memory tracing runs periodically in production on end-user
devices, so unverified or heavy dump overhead degrades browser performance.
Dumps not matching allowlist patterns are dropped, and inconsistent naming or
omitted backend variants fragment metric hierarchies and cause silent telemetry
loss.

## Examples

Bad:

```cpp
// base/trace_event/memory_infra_background_allowlist.cc
// Adding unverified high-overhead dump, non-canonical name, or incomplete backend variants:
constexpr auto kAllocatorDumpNameAllowlist =
    base::MakeFixedFlatSet<std::string_view>({
        "sqlite/IndexedDB_connection/0x?",       // Unverified / high overhead
        "site_storage/index_db/sqlite_0x?",      // Non-canonical name
        "site_storage/localstorage/0x?/leveldb",
        "site_storage/sessionstorage/0x?/sqlite", // Missing leveldb counterpart
    });
```

Good:

```cpp
// base/trace_event/memory_infra_background_allowlist.cc
// Verified low-overhead dumps with canonical naming and complete backend coverage:
constexpr auto kAllocatorDumpNameAllowlist =
    base::MakeFixedFlatSet<std::string_view>({
        "site_storage/indexed_db/sqlite_0x?",
        "site_storage/localstorage/0x?/leveldb",
        "site_storage/localstorage/0x?/sqlite",
        "site_storage/sessionstorage/0x?/leveldb",
        "site_storage/sessionstorage/0x?/sqlite",
    });
```
