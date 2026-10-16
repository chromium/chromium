---
name: general/document-api-availability-by-capability-not-flag-name
rubric: readability
severity: nit
---

**Rule.** When documenting API methods, factory functions, capability checks, or
platform delegates, describe the high-level goal and caller-facing behavioral
contract across operational states—such as supported operations, parameter
semantics, fallback or failure behavior when unsupported (e.g., thrown
exceptions, error callbacks, or sentinel defaults), nesting/re-entrancy support,
and threading constraints. Do not merely cite underlying delegated platform
method signatures, internal implementation types (e.g., `*Impl`), or internal/OS
flag names.

**Why.** Callers rely on doc comments to understand what an API accomplishes and
how to safely invoke it across different states without inspecting internal
source code. Merely referencing internal class names, flag names, or underlying
delegated framework methods leaks implementation details and creates brittle
comments that fail to explain the abstraction's contract to consumers.

## Examples

Bad:

```cpp
// Variant 1: Referencing internal implementation classes instead of behavior across states.
/**
 * Returns a ScopedServiceBindingBatch.
 *
 * <p>If the feature is activated, this returns a ScopedServiceBindingBatchImpl. Otherwise, this
 * returns a ScopedServiceBindingBatchDuration to measure the duration of the batching scope or
 * null.
 */
static @Nullable ScopedServiceBindingBatch scoped();

// Variant 2: Referencing internal flag names instead of caller-facing capabilities.
/**
 * Returns whether features under `android.content.flags.enable_update_service_bindings` are
 * available or not.
 */
default boolean isUpdateServiceBindingApiAvailable() {
    return false;
}

// Variant 3: Merely citing underlying platform method without explaining caller contract or intent.
/**
 * Calls the {@link
 * android.app.ContentRestrictionManager#requestClassification(ClassifiableContent, Executor,
 * OutcomeReceiver<Boolean, Exception>)} method.
 */
default Promise<Boolean> requestContentRestrictionClassification(...)

// Variant 4: Omitting fallback or failure behavior when unsupported.
/**
 * Calls the {@link AppTask#requestWindowingLayer} method if supported.
 */
@RequiresApi(Build.VERSION_CODES.S)
default void requestPinnedWindowingLayer(
        AppTask appTask, Executor executor, OutcomeReceiver<Void, Exception> receiver) {
    receiver.onError(new UnsupportedOperationException("Not supported"));
}
```

Good:

```cpp
// Variant 1: Describing behavioral contract across states, re-entrancy, and thread constraints.
/**
 * Returns a ScopedServiceBindingBatch.
 *
 * <p>If the feature was previously activated (via tryActivate()): Returns an instance that will
 * cause bindings to be batched until close() is called. Nested calls are supported.
 *
 * <p>If the feature was not activated: Return an instance that just acts as a timer. Nested
 * calls will return null.
 *
 * <p>Must be called on the same thread as the thread that called tryActivate().
 */
static @Nullable ScopedServiceBindingBatch scoped();

// Variant 2: Describing supported operations and caller-facing capabilities.
/**
 * Returns whether rebindService() is available or not.
 */
default boolean isUpdateServiceBindingApiAvailable() {
    return false;
}

// Variant 3: Documenting high-level goal, caller-facing contract, and return outcome.
/**
 * Calls the platform to determine if the content should be allowed or blocked.
 *
 * @return A promise fulfilled with the boolean classification result (true if allowed),
 *     rejected otherwise with {@link UnsupportedOperationException} if not supported...
 */
default Promise<Boolean> requestContentRestrictionClassification(...)

// Variant 4: Documenting explicit failure/fallback behavior when unsupported.
/**
 * Calls the {@link AppTask#requestWindowingLayer} method if supported. If not supported,
 * the receiver will be notified with an error {@link UnsupportedOperationException}.
 */
@RequiresApi(Build.VERSION_CODES.S)
default void requestPinnedWindowingLayer(
        AppTask appTask, Executor executor, OutcomeReceiver<Void, Exception> receiver) {
    receiver.onError(new UnsupportedOperationException("Not supported"));
}
```
