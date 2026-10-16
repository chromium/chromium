---
name: general/assert-span-bounds-within-backing-buffer
rubric: correctness
severity: should_fix
---

**Rule.** When constructing a `base::span` from a raw pointer and length into an
underlying backing buffer—especially when wrapping external C or OS structures
like `CMSG_DATA`—assert that the resulting span lies within the backing buffer
bounds. For lower bound containment assertions, use `CHECK_GE` (or `DCHECK_GE`)
rather than `CHECK_GT` (or `DCHECK_GT`), as a valid sub-range or payload may
begin at offset 0 of the backing buffer.

**Why.** Constructing spans from raw pointers inside `UNSAFE_BUFFERS` relies on
manual pointer arithmetic or externally provided lengths. Asserting bounds with
`CHECK_GE` and `CHECK_LE` verifies memory safety while preventing false
assertion failures when payloads begin at offset 0.

## Examples

Bad:

```cpp
// SAFETY: cmsg->cmsg_len is set by the kernel and is at most the size
// of the `msg_controllen` buffer.
wire_fds =
    UNSAFE_BUFFERS(base::span(reinterpret_cast<int*>(CMSG_DATA(cmsg)),
                              payload_len / sizeof(int)));
// Incorrect: CHECK_GT causes false assertion failures when wire_fds starts at offset 0.
CHECK_GT(wire_fds.data(), reinterpret_cast<int*>(control_buffer));
CHECK_LE(&wire_fds.back(),
         reinterpret_cast<int*>(&control_buffer[kControlBufferSize - sizeof(int)]));
```

Good:

```cpp
// SAFETY: cmsg->cmsg_len is set by the kernel and is at most the size
// of the `msg_controllen` buffer.
wire_fds =
    UNSAFE_BUFFERS(base::span(reinterpret_cast<int*>(CMSG_DATA(cmsg)),
                              payload_len / sizeof(int)));
// Correct: CHECK_GE safely accommodates sub-spans or payloads starting at offset 0.
CHECK_GE(wire_fds.data(), reinterpret_cast<int*>(control_buffer));
CHECK_LE(&wire_fds.back(),
         reinterpret_cast<int*>(&control_buffer[kControlBufferSize - sizeof(int)]));
```
