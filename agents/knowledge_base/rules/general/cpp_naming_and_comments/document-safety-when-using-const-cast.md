---
name: general/document-safety-when-using-const-cast
rubric: readability
severity: nit
---

**Rule.** When `const_cast` is unavoidable (such as when bridging to external C
APIs or performing address-only bounds checks on cv-qualified pointers), add an
explanatory or `// SAFETY:` comment documenting why casting away the qualifier
is safe (e.g., verifying that underlying memory is mutable, the callee does not
mutate the buffer, or pointed-to data is never dereferenced). In the comment,
explicitly distinguish whether qualifiers apply to the pointed-to data (e.g.,
`const volatile void*`) versus the pointer variable itself (`void* volatile`).

**Why.** Casting away `const` or `volatile` removes compiler-enforced
immutability and memory-access guarantees, risking undefined behavior if
read-only data is mutated or volatile accesses are elided. Documenting whether
volatility or constness belongs to the pointed-to object versus the pointer
variable ensures reviewers can verify that the cast is benign and safe.

## Examples

Bad:

```cpp
// Bad: Casting away const without explaining why the underlying buffer is safe:
rv = posix_spawnp(
    &pid, executable_path, file_actions.get(), attr.get(), &argv_cstr[0],
    owned_environ.empty() ? const_cast<char**>(new_environ.data())
                          : owned_environ.data());

// Bad: Conflating pointee volatility with pointer variable volatility:
// This const cast is safe because we never actually dereference
// `ptr`. Therefore we don't care that it's volatile.
return IsExtentOutOfBounds(const_cast<const void*>(ptr), extent_bytes,
                           type_size);
```

Good:

```cpp
// Good: Explaining why removing const is safe for mutable memory:
// SAFETY: `new_environ.data()` points to the system's `environ` array,
// which is actually mutable (`char**`). `posix_spawnp` does not modify
// the envp array or the strings it points to, so casting away constness
// to match the posix_spawnp signature is safe.
rv = posix_spawnp(
    &pid, executable_path, file_actions.get(), attr.get(), &argv_cstr[0],
    owned_environ.empty() ? const_cast<char**>(new_environ.data())
                          : owned_environ.data());

// Good: Accurately distinguishing that pointed-to data is volatile:
// This const cast is safe because we never actually dereference
// `ptr`. Therefore we don't care that it points to volatile.
return IsExtentOutOfBounds(const_cast<const void*>(ptr), extent_bytes,
                           type_size);
```
