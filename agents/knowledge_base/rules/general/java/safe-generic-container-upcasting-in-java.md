---
name: general/safe-generic-container-upcasting-in-java
rubric: correctness
severity: should_fix
---

**Rule.** Do not use an unconstrained generic return type with a wildcard
parameter (such as <S> Container<S> asBase(Class\<? super T> clazz)) to upcast
generic containers in Java. Because Java type parameter declarations only
support upper bounds, enforce compile-time type safety for container upcasting
using a static helper method with upper-bounded type parameters \<BaseT, ChildT
extends BaseT>.

**Why.** Java generic type parameters do not support lower bounds (<S super T>
is invalid syntax). Declaring <S> Container<S> method(Class\<? super T> clazz)
leaves S completely unconstrained and disconnected from clazz, allowing the
compiler to unsafely cast the container to any arbitrary type at call sites. A
static method parameterized by \<BaseT, ChildT extends BaseT> enforces strict
subtype validation at compile time.

## Examples

Bad:

```cpp
// S is unbounded and disconnected from clazz, allowing unsafe casts
// such as SyncOneshotSupplier<Integer> s = strSupplier.asSupplierOf(String.class);
default <S> SyncOneshotSupplier<S> asSupplierOf(Class<? super T> clazz) {
    return (SyncOneshotSupplier<S>) (SyncOneshotSupplier<?>) this;
}
```

Good:

```cpp
// Enforces compile-time type safety via <BaseT, ChildT extends BaseT>
static <BaseT, ChildT extends BaseT>
        MonotonicObservableSupplier<BaseT> convertToSupplierOfBaseClass(
                MonotonicObservableSupplier<ChildT> s, Class<BaseT> baseClass) {
    return (MonotonicObservableSupplier<BaseT>) s;
}
```
