---
name: general/use-int64-for-profiler-metadata
rubric: readability
severity: should_fix
---

**Rule.** When accepting, storing, or plumbing values and opaque identifiers
(such as enum values or thread group IDs) destined for profiler metadata APIs
(e.g., base::SampleMetadata, base::ScopedSampleMetadata, or
base::AddProfileMetadataForThread), declare intermediate constructor parameters,
methods, and member variables as int64_t rather than narrower types like int or
uint32_t.

**Why.** Profiler metadata APIs record keys and values as int64_t. Using int64_t
throughout intermediate constructors and class interfaces prevents type
mismatches, avoids implicit conversions, and ensures consistency with the
underlying metadata recording API contract.

## Examples

Bad:

```cpp
// BAD: Uses int for a parameter and member variable passed to SampleMetadata
class ThreadGroupProfiler {
 public:
  explicit ThreadGroupProfiler(
      scoped_refptr<SequencedTaskRunner> task_runner,
      int thread_group_type);

 private:
  const int thread_group_type_;
};
```

Good:

```cpp
// GOOD: Uses int64_t consistently to match profiler metadata API types
class ThreadGroupProfiler {
 public:
  explicit ThreadGroupProfiler(
      scoped_refptr<SequencedTaskRunner> task_runner,
      int64_t thread_group_type);

 private:
  const int64_t thread_group_type_;
};
```
