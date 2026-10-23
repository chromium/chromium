---
name: components/sync/check-sync-delegate-non-null-before-controller-creation
description: Use when sync controller builders (e.g. CommonControllerBuilder) get a DataTypeControllerDelegate from an optional service or bridge to create a DataTypeController or ForwardingDataTypeControllerDelegate.
rubric: correctness
severity: blocking
---

**Rule.** In sync controller builders (such as `CommonControllerBuilder`),
verify that optional service dependencies (e.g., `service.value()`) are non-null
before dereferencing them to retrieve a `DataTypeControllerDelegate`.
Additionally, verify that the delegate pointer or weak pointer returned by the
service or bridge is non-null before constructing a `DataTypeController` or
passing it to `ForwardingDataTypeControllerDelegate`.

**Why.** Keyed services may not be instantiated for certain profiles (such as
guest or off-the-record) or platforms, leaving optional service pointers null.
Furthermore, sync bridges and delegate getters can return null when disabled or
uninitialized. Dereferencing null service pointers causes immediate crashes,
while passing null delegates into `DataTypeController` or
`ForwardingDataTypeControllerDelegate` violates non-null preconditions and
crashes during sync initialization.

## Examples

Bad:

```cpp
// Anti-pattern: dereferencing optional service without null check,
// and passing unchecked nullable delegate to controller constructor.
if (!disabled_types.Has(syncer::GEMINI_THREAD) &&
    base::FeatureList::IsEnabled(syncer::kSyncGeminiThread)) {
  syncer::DataTypeControllerDelegate* delegate =
      contextual_tasks_service_.value()
          ->GetGeminiThreadControllerDelegate()
          .get();
  controllers.push_back(std::make_unique<DataTypeController>(
      syncer::GEMINI_THREAD,
      std::make_unique<syncer::ForwardingDataTypeControllerDelegate>(delegate),
      std::make_unique<syncer::ForwardingDataTypeControllerDelegate>(delegate)));
}
```

Good:

```cpp
if (!disabled_types.Has(syncer::GEMINI_THREAD) &&
    base::FeatureList::IsEnabled(syncer::kSyncGeminiThread) &&
    contextual_tasks_service_.value()) {
  syncer::DataTypeControllerDelegate* delegate =
      contextual_tasks_service_.value()
          ->GetGeminiThreadControllerDelegate()
          .get();
  if (delegate) {
    controllers.push_back(std::make_unique<DataTypeController>(
        syncer::GEMINI_THREAD,
        std::make_unique<syncer::ForwardingDataTypeControllerDelegate>(delegate),
        std::make_unique<syncer::ForwardingDataTypeControllerDelegate>(delegate)));
  }
}
```
