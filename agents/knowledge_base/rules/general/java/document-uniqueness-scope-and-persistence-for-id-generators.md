---
name: general/document-uniqueness-scope-and-persistence-for-id-generators
rubric: readability
severity: nit
---

**Rule.** When documenting an ID generator, sequence generator, or identifier
allocator class or method, explicitly state its uniqueness scope (e.g., unique
within a single process vs. cross-process) and lifetime or persistence
guarantees (e.g., whether IDs persist across application restarts). Avoid vague
or unqualified claims of 'unique IDs' without clarifying the scope boundaries.

**Why.** Unqualified claims of 'unique IDs' are ambiguous and can lead callers
to mistakenly assume IDs are globally unique across processes or persist across
storage/app restarts. Documenting process-local scope and lifetime prevents
incorrect assumptions about cross-process IPC, serialization, and storage
durability.

## Examples

Bad:

```cpp
/** For generating a sequence of unique IDs. ID generation is thread-safe. */
public final class IdSequencer {
    public static long next() {
        return sNextId.getAndIncrement();
    }
}
```

Good:

```cpp
/**
 * Generates a sequence of IDs that will be unique within a process, and will not persist across app
 * restarts. ID generation is thread-safe.
 */
public final class IdSequencer {
    public static long next() {
        return sNextId.getAndIncrement();
    }
}
```
