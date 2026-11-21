---
name: general/prefer-jni-pointer-wrappers-over-long-native-pointers
rubric: safety
severity: should_fix
---

**Rule.** In new or migrated Java-to-native bridges, hold the C++ object as a
typed `JniUniquePtr<NativeFoo>`, `JniRawPtr<NativeFoo>` or `JniPtr<NativeFoo>`,
where `NativeFoo` is an interface that extends `JniTypeToken` and is annotated
with `@JniType("::foo::Foo")`. Declare the receiver of a `@NativeMethods` member
call as `JniPtr<NativeFoo> self`, not `long nativeFoo`. Call `destroy()` on a
`JniUniquePtr` (or `release()` on a `JniRawPtr`) on every path that ends the
owner's lifetime, including in tests.

**Why.** `third_party/jni_zero/README.md` calls raw `long` addresses a legacy
pattern that bypasses compile-time type checks and use-after-free protections,
and asks for safe pointers in new and migrated code. In debug builds the
wrappers use `LifetimeAssert`, so a wrapper that is garbage collected without
being destroyed or released crashes
(`third_party/jni_zero/docs/safe_jni_pointers.md`).

## Examples

Bad:

```java
private long mNativeFooBridge;

@NativeMethods
interface Natives {
    long init();
    void update(long nativeFooBridge, int value);
}
```

Good:

```java
@JniType("::foo::FooBridge")
interface NativeFooBridge extends JniTypeToken {}

private @Nullable JniUniquePtr<NativeFooBridge> mNativeFooBridge;

@NativeMethods
interface Natives {
    JniUniquePtr<NativeFooBridge> init();
    void update(JniPtr<NativeFooBridge> self, int value);
}
```
