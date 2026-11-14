# Review Rules: Remove Unused Object Overrides

- **Evidence Check:** Every kept `equals()`, `hashCode()`, or `toString()` in a
  touched or skipped class must cite the file and the code that calls that
  specific method, directly or implicitly (blanket-blocked classes, i.e.
  native-annotated in the class itself, domain models, or outside the UI scope,
  may cite the annotation or category instead). Every removed override must list
  the full usage checklist with results. Flag a kept override as a missed
  removal when its cited code doesn't exist or doesn't make that method run on
  an instance of the class: constructing it, reading its fields, or passing it
  to code that never compares, hashes, or prints it doesn't count, while passing
  it to an `assertEquals` or Mockito `verify` comparing two different instances,
  `contains`, a map or set key, or `PropertyModel.set` with a non-skip-equality
  key does. Flag a removal with an incomplete checklist as unsafe.
- **Domain & Native Check:** Add hard blockers for domain models and classes
  with `@CalledByNative`, `@NativeMethods`, or `@JniType`. A domain model is
  persisted (prefs, files, database, `Bundle`/`Parcel`), crosses a process
  boundary (Mojo, AIDL, Binder, WebView support library), or lives under
  `components/autofill`, `components/payments`, `components/password_manager`,
  `components/sync`, or `components/signin` (the same word elsewhere in a path
  doesn't count); the block must cite which criterion applies with the file and
  code that shows it.
- **Hierarchy Check:** Block removing an override if a superclass (other than
  `Object`) implements the method or a superclass or interface declares it,
  because removal changes which method runs.
- **Symmetry Check (`equals()` and `hashCode()` Pair):** Verify that no modified
  class defines `equals()` without `hashCode()`, or `hashCode()` without
  `equals()`. Both methods must be removed together or kept together.
- **Collection & Hashing Audit:** Block removing `equals()` if instances are
  searched via collections. Verify the class is not inserted into hashed
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
- **Test Integrity:** Treat broken JUnit assertions or Mockito `verify()` calls
  as evidence of value equality. Block rewriting them with `refEq(expected)` or
  custom field assertion helpers.
- **Import Cleanup:** Verify unused imports (e.g., `java.util.Objects`,
  `java.util.Locale`) resulting from method removals have been cleaned up.
