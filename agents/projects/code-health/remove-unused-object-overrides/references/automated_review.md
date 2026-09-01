# Automated Review Protocol: Remove Unused Object Overrides

Use this prompt when delegating a final review of a
`remove-unused-object-overrides` patch to the **`generalist`** sub-agent.

## Review Prompt

Delegate the review to the **`generalist`** sub-agent with this exact prompt:

> "Follow the **Shared Automated Review Protocol** in
> `../../hub/references/automated_review.md`.
>
> In addition to the generic checks, you MUST evaluate the patch against these
> **Specific Cleanup Rules** for Remove Unused Object Overrides:
>
> - **Symmetry Check (`equals()` and `hashCode()` Pair):** Verify that no
>   modified class defines `equals()` without `hashCode()`, or `hashCode()`
>   without `equals()`. Both methods must be removed together or kept together.
> - **Collection & Hashing Audit:** Verify the class is not inserted into hashed
>   collections (`HashMap`, `HashSet`, `LinkedHashMap`, `LinkedHashSet`,
>   `ConcurrentHashMap`), searched via collection methods (`List.contains()`,
>   `List.indexOf()`, `List.remove()`, `Collection.removeAll()`), or hashed
>   directly/indirectly (`obj.hashCode()`, `Objects.hashCode()`,
>   `Arrays.hashCode()`).
> - **Equality & State Comparison:** Verify that `Objects.equals(a, b)` or
>   `a.equals(b)` is not called in production code (e.g. for snapshot/state
>   comparison).
> - **Serialization & Reflection:** Verify the class is not serialized via
>   Proto, Mojo, JSON, or reflection-based caches.
> - **Test Integrity:** If Mockito `verify()` calls or JUnit assertions were
>   broken by removal, confirm they were appropriately updated with
>   `refEq(expected)` or explicit field assertions/helpers.
> - **Import Cleanup:** Verify unused imports (e.g., `java.util.Objects`,
>   `java.util.Locale`) resulting from method removals have been cleaned up."
