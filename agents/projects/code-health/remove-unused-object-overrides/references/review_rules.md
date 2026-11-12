# Review Rules: Remove Unused Object Overrides

- **Symmetry Check (`equals()` and `hashCode()` Pair):** Verify that no modified
  class defines `equals()` without `hashCode()`, or `hashCode()` without
  `equals()`. Both methods must be removed together or kept together.
- **Collection & Hashing Audit:** Verify the class is not inserted into hashed
  collections (`HashMap`, `HashSet`, `LinkedHashMap`, `LinkedHashSet`,
  `ConcurrentHashMap`), searched via collection methods (`List.contains()`,
  `List.indexOf()`, `List.remove()`, `Collection.removeAll()`), or hashed
  directly/indirectly (`obj.hashCode()`, `Objects.hashCode()`,
  `Arrays.hashCode()`).
- **Equality & State Comparison:** Verify that `Objects.equals(a, b)` or
  `a.equals(b)` is not called in production code (e.g. for snapshot/state
  comparison).
- **Serialization & Reflection:** Verify the class is not serialized via Proto,
  Mojo, JSON, or reflection-based caches.
- **Test Integrity:** If Mockito `verify()` calls or JUnit assertions were
  broken by removal, confirm they were appropriately updated with
  `refEq(expected)` or explicit field assertions/helpers.
- **Import Cleanup:** Verify unused imports (e.g., `java.util.Objects`,
  `java.util.Locale`) resulting from method removals have been cleaned up.
