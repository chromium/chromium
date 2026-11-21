---
name: general/omit-redundant-null-asserts-on-jni-pointer-receivers
rubric: readability
severity: nit
---

**Rule.** Do not add `assert mNativeFoo != null;` or `assertNonNull()` before
passing a `JniUniquePtr`, `JniRawPtr` or `JniPtr` receiver to a `@NativeMethods`
call. The generated proxy already asserts that the receiver is non-null; for a
`@Nullable` field, `assumeNonNull(mNativeFoo)` at the call site is enough to
satisfy NullAway. The same proxy also asserts every non-primitive parameter that
is not `@Nullable`, so those need no extra assert either.

**Why.** The JNI Zero generator
(`third_party/jni_zero/codegen/proxy_impl_java.py`) emits these assertions in
every proxy method. A hand-written assert duplicates the generated one and adds
the same noise to every pointer-wrapper migration.

## Examples

Bad:

```java
assert mNativeBridge != null;
FooBridgeJni.get().update(assumeNonNull(mNativeBridge), value);
```

Good:

```java
FooBridgeJni.get().update(assumeNonNull(mNativeBridge), value);
```
