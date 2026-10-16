---
name: general/use-identity-collections-for-mockito-mocks
rubric: correctness
severity: should_fix
---

**Rule.** When storing or tracking arbitrary Java object instances where
reference identity matters (such as listeners, observers, resources, or
lifecycle resetters) or storing Mockito mocks in collections, use an
identity-based collection like Collections.newSetFromMap(new
IdentityHashMap\<>()) or IdentityHashMap instead of a standard HashSet or
HashMap.

**Why.** Standard hash collections invoke .equals() and .hashCode(), which
collapse distinct instances that compare equal or share equality behavior.
Additionally, invoking .equals() or .hashCode() on Mockito mocks records
spurious method interactions, interferes with verification, or crashes if mocked
classes define final methods accessing uninitialized state. Wrapping
IdentityHashMap via Collections.newSetFromMap(...) provides a Set that strictly
uses reference equality (==) and System.identityHashCode() without method
dispatch.

## Examples

Bad:

```cpp
// HashSet relies on .hashCode() and .equals(), which collapses equal instances or invokes methods on mocks.
private static final Set<Object> sMocksToReset = new HashSet<>();
private final Set<Listener> mListeners = new HashSet<>();
```

Good:

```cpp
// Identity-based set uses System.identityHashCode() and reference equality (==) without invoking methods.
private static final Set<Object> sMocksToReset =
        Collections.newSetFromMap(new IdentityHashMap<>());
private final Set<Listener> mListeners =
        Collections.newSetFromMap(new IdentityHashMap<>());
```
