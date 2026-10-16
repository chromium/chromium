---
name: general/prefer-unsafe-buffers-over-convoluted-subspans-at-c-api-boun
rubric: readability
severity: should_fix
---

**Rule.** When wrapping C-style or OS API buffers (such as POSIX CMSG_DATA) into
spans, directly construct base::span using UNSAFE_BUFFERS(...) accompanied by an
explanatory // SAFETY: comment documenting the backing memory and bounds, rather
than computing convoluted subspan offsets and pointer assertions from the
backing container.

**Why.** Artificially calculating subspan offsets and asserting pointer equality
just to avoid UNSAFE_BUFFERS at external C or OS boundaries hurts readability
and introduces fragility. Interacting with external C-style APIs is an
explicitly sanctioned use of UNSAFE_BUFFERS in Chromium when paired with a clear
// SAFETY: comment.

## Examples

Bad:

```cpp
// Attempting complex subspan offset math and pointer equality checks
// to artificially avoid UNSAFE_BUFFERS:
base::span<char> fd_span =
    control_buffer.as_span().subspan(CMSG_LEN(0), sizeof(int) * fds.size());
CHECK_EQ(UNSAFE_BUFFERS(CMSG_DATA(cmsg)),
         reinterpret_cast<unsigned char*>(fd_span.data()));
fd_span.copy_from(base::as_chars(base::span(fds)));
```

Good:

```cpp
// SAFETY: CMSG_DATA is backed by `control_buffer` above and is of size
// `control_len`. See https://man7.org/linux/man-pages/man3/cmsg.3.html
auto control_span = UNSAFE_BUFFERS(
    base::span(CMSG_DATA(cmsg), sizeof(int) * fds.size()));
control_span.copy_from_nonoverlapping(base::as_bytes(base::span(fds)));
```
