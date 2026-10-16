---
name: general/remove-dead-parameters-during-refactoring
rubric: readability
severity: should_fix
---

**Rule.** Do not preserve, forward, or mechanically modernize unused helper
functions, concepts, type traits/aliases, dead methods in classes or test
fixtures, unused parameters, dead branches, obsolete constructor or method
overloads, obsolete enums, or remnants of retired features. When updating API
signatures, touching legacy code, or deleting legacy implementations, audit base
classes, interfaces, sibling subclasses, and template utilities to cleanly
delete unreferenced concepts, helper templates, orphaned interface methods,
no-op overrides, obsolete overloads, dead parameters, orphaned state, and
redundant forwarding wrappers—updating remaining callers and inlining underlying
calls directly—and eliminate vestigial warning logs and stale comments.

**Why.** Preserving obsolete overloads, unused helpers, dead concepts,
unreferenced type traits, dead parameters, orphaned state, trivial forwarding
wrappers, or vestigial warnings creates technical debt, clutters headers, bloats
interfaces and test fixtures, and misleads maintainers with stale abstractions
instead of standardizing on updated APIs.

## Examples

Bad:

```cpp
// Mechanically adapting an unused parameter during spanification:
static HistogramBase* FactoryGetWithRangeDescription(
    std::string_view name,
    Sample32 minimum,
    Sample32 maximum,
    size_t bucket_count,
    int32_t flags,
    base::span<const DescriptionPair> descriptions);

return FactoryGetWithRangeDescription(name, minimum, maximum, bucket_count, flags, {});

// Retaining dead interface methods and no-op overrides after deleting an implementation:
class ThreadController {
  virtual void RestoreDefaultTaskRunner() = 0;
};
class ThreadControllerWithMessagePumpImpl : public ThreadController {
  void RestoreDefaultTaskRunner() override {}  // No-op for this subclass.
};

// Retaining an unused concept or template helper after refactoring:
template <typename T>
concept IsJniCallback =
    base::android::internal::IsOnceCallback<std::remove_cvref_t<T>>::value ||
    base::android::internal::IsRepeatingCallback<std::remove_cvref_t<T>>::value;
```

Good:

```cpp
// Remove unused parameters, dead infrastructure, and obsolete overloads:
static HistogramBase* FactoryGetWithRangeDescription(
    std::string_view name,
    Sample32 minimum,
    Sample32 maximum,
    size_t bucket_count,
    int32_t flags);

return FactoryGetWithRangeDescription(name, minimum, maximum, bucket_count, flags);

// Cleanly delete orphaned interface methods and no-op overrides when removing the implementation:
// (RestoreDefaultTaskRunner removed from ThreadController and ThreadControllerWithMessagePumpImpl)

// Remove dead concepts, type traits, and helper templates when callers are removed:
// (Unused `IsJniCallback` concept removed cleanly)
```
