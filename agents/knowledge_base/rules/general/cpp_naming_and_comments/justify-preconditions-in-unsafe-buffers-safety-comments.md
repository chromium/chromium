---
name: general/justify-preconditions-in-unsafe-buffers-safety-comments
rubric: readability
severity: should_fix
---

**Rule.** Colocate each UNSAFE_BUFFERS(...) invocation or base::unchecked span
construction directly below a dedicated // SAFETY: comment justifying why the
operation is safe for all inputs. In the comment, explain how caller
preconditions are satisfied (citing invariants, bounds checks, or header
contracts) rather than merely restating callee requirements; cite links to
official man pages or specifications when relying on external OS/C APIs; and
document performance exemption rationales for base::unchecked. When moving or
refactoring unsafe operations, keep the safety comment paired directly with the
macro invocation.

**Why.** An orphaned, separated, or uninformative // SAFETY: comment leaves
UNSAFE_BUFFERS operations undocumented and code reviews unable to verify
correctness. Reviewers and maintainers need caller-side justification—including
bounds invariants, external man page citations, or profiling data for
base::unchecked—located directly adjacent to the unsafe operation.

## Examples

Bad:

```cpp
// Variant 1: UNSAFE_BUFFERS separated from its SAFETY comment during refactoring
auto map = UNSAFE_BUFFERS(base::span<uint8_t>(static_cast<uint8_t*>(ptr), len));
// SAFETY: Valid because data_offset + byte_size <= len.
bytes_ = map.subspan(data_offset, byte_size);

// Variant 2: Merely restating callee requirements instead of caller invariants
// SAFETY: AlterEnvironment requires each string in 'env' to be null-terminated.
base::HeapArray<char*> env = UNSAFE_BUFFERS(
    internal::AlterEnvironment(old_env, env_modifications));

// Variant 3: Missing reference link to external OS / man page documentation
// SAFETY: CMSG_NXTHDR uses cmsg_len set by kernel within msg_controllen buffer.
for (cmsg = CMSG_FIRSTHDR(&msg); cmsg;
     cmsg = UNSAFE_BUFFERS(CMSG_NXTHDR(&msg, cmsg))) {}

// Variant 4: Omitting performance exemption rationale for base::unchecked
// SAFETY: Points to kThreadLocalStorageSize elements in tls_vector.
base::span<TlsVectorEntry> UNSAFE_BUFFERS(tls_data(
    base::unchecked, tls_vector, tls_vector ? kThreadLocalStorageSize : 0));
```

Good:

```cpp
// Variant 1: SAFETY comment colocated immediately above UNSAFE_BUFFERS
// SAFETY: ptr points to `len` bytes mapped via mmap(); data_offset + byte_size <= len.
auto map = UNSAFE_BUFFERS(base::span<uint8_t>(static_cast<uint8_t*>(ptr), len));
bytes_ = map.subspan(data_offset, byte_size);

// Variant 2: Justifying caller invariant via producer header contract
// SAFETY: AlterEnvironment() requires null-terminated strings; GetEnvironment()
// guarantees in its header contract that each returned string is null-terminated.
base::HeapArray<char*> env = UNSAFE_BUFFERS(
    internal::AlterEnvironment(old_env, env_modifications));

// Variant 3: Linking external API documentation / man page
// SAFETY: CMSG_NXTHDR uses cmsg_len set by kernel within msg_controllen buffer.
// See https://linux.die.net/man/3/cmsg_nxthdr
for (cmsg = CMSG_FIRSTHDR(&msg); cmsg;
     cmsg = UNSAFE_BUFFERS(CMSG_NXTHDR(&msg, cmsg))) {}

// Variant 4: Explaining memory safety and profiling rationale for base::unchecked
// SAFETY: Points to kThreadLocalStorageSize elements in tls_vector. Constructing
// this span is hot in Speedometer3 profiling; exempt from Checked Span bounds checks.
base::span<TlsVectorEntry> UNSAFE_BUFFERS(tls_data(
    base::unchecked, tls_vector, tls_vector ? kThreadLocalStorageSize : 0));
```
