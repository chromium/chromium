---
name: general/include-target-receiver-in-platform-delegate-methods
rubric: correctness
severity: should_fix
---

**Rule.** When declaring delegate, dispatcher, or shim interface methods (such
as AconfigFlaggedApiDelegate) that forward or wrap instance methods of Android
framework classes, include the target receiver instance in the method signature
so the delegate implementation can invoke the operation on the receiver. Use
AndroidX compat wrapper types (such as AccessibilityNodeInfoCompat or
ViewCompat) rather than bare framework classes where available to ensure
backwards compatibility across supported Android versions.

**Why.** Delegate interfaces are typically stateless or singleton dispatchers
without direct references to caller objects. Omitting the target receiver
parameter prevents delegate implementations from executing the instance method
on the target object, while using bare framework types instead of AndroidX
compat wrappers bypasses Chromium's backwards compatibility layer on older
Android releases.

## Examples

Bad:

```cpp
// Anti-pattern: Missing receiver object prevents delegate from invoking instance method
public interface AconfigFlaggedApiDelegate {
    default void setSelection(Selection selection) {}
}
```

Good:

```cpp
// Idiomatic: Passes the target receiver using AndroidX compat wrapper
public interface AconfigFlaggedApiDelegate {
    default void setSelection(
            AccessibilityNodeInfoCompat info, @Nullable Selection selection) {}
}
```
